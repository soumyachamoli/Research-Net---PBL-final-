#include "HttpUtils.h"
#include "TextUtils.h"
#include "WebServer.h"
using namespace std;
using json = nlohmann::json;

static PaperFilter readFilter(const httplib::Request& req)
{
    PaperFilter filter;
    filter.author = textParam(req, "author", 100);
    filter.journal = textParam(req, "journal", 200);
    filter.category = textParam(req, "category", 100);
    filter.keyword = textParam(req, "keyword", 100);
    filter.type = textParam(req, "type", 50);
    filter.year = intParam(req, "year", 0, 1000, 2100);
    filter.yearFrom = intParam(req, "year_from", 0, 1000, 2100);
    filter.yearTo = intParam(req, "year_to", 0, 1000, 2100);
    filter.minImpact = numberParam(req, "min_impact", 0, 100000);
    filter.maxImpact = numberParam(req, "max_impact", 0, 100000);
    filter.page = intParam(req, "page", 1, 1, 10000);
    filter.pageSize = intParam(req, "page_size", 10, 1, 50);
    string sort = textParam(req, "sort", 20);
    if (!sort.empty())
    {
        filter.sort = sort;
    }

    if (!isValidSort(filter.sort))
    {
        throw BadRequest("sort must be one of: relevance, newest, oldest, impact_desc, impact_asc, citations, author");
    }
    if (filter.yearFrom > 0 && filter.yearTo > 0 && filter.yearFrom > filter.yearTo)
    {
        throw BadRequest("year_from must not be after year_to");
    }
    if (filter.minImpact && filter.maxImpact && *filter.minImpact > *filter.maxImpact)
    {
        throw BadRequest("min_impact must not be larger than max_impact");
    }
    return filter;
}

void WebServer::registerPaperRoutes()
{
    server_.Get("/api/health", [this](const httplib::Request&, httplib::Response& res) {
        auto engine = engines_.get();
        try
        {
            db_.query("SELECT 1");
        }
        catch (const DatabaseError&)
        {
            sendJson(res, 503, {{"status", "unavailable"}, {"database", "unreachable"}, {"papers", engine->papers().size()}});
            return;
        }
        sendJson(res, 200, {{"status", "ok"}, {"database", "ok"}, {"papers", engine->papers().size()}});
    });

    server_.Get("/api/stats", [this](const httplib::Request&, httplib::Response& res) {
        auto engine = engines_.get();
        sendJson(res, 200, {
            {"papers", engine->papers().size()},
            {"indexedTerms", engine->index().termCount()},
            {"trieNodes", engine->trieNodes()},
            {"yearFrom", engine->minYear()},
            {"yearTo", engine->maxYear()},
        });
    });

    auto search = [this](const httplib::Request& req, httplib::Response& res) {
        string query = textParam(req, "q", 300);
        PaperFilter filter = readFilter(req);
        int userId = currentUserId(req);

        auto engine = engines_.get();
        bool hasQuery = !analyze(query).empty();
        bool relaxed = false;
        vector<SearchHit> hits;
        if (hasQuery)
        {
            hits = engine->search(query);
            vector<string> words = tokenize(query);
            if (hits.empty() && words.size() > 1)
            {
                string anyWord;
                for (const string& word : words)
                {
                    anyWord += (anyWord.empty() ? "" : " or ") + word;
                }
                hits = engine->search(anyWord);
                relaxed = !hits.empty();
            }
        }
        json result = papers_.search(hasQuery ? &hits : nullptr, filter, userId);
        result["query"] = query;
        result["relaxed"] = relaxed;
        result["filters"] = filter.toJson();
        onSearch(userId, query, filter, result["total"].get<long>());
        sendJson(res, 200, result);
    };
    server_.Get("/api/search", search);
    server_.Get("/api/papers", search);

    server_.Get("/api/autocomplete", [this](const httplib::Request& req, httplib::Response& res) {
        string prefix = textParam(req, "prefix", 100);
        int limit = intParam(req, "limit", 8, 1, 10);
        json list = json::array();
        for (const Suggestion& s : engines_.get()->autocomplete(prefix, limit))
        {
            json item = {{"text", s.text}, {"kind", s.kind}};
            if (s.paperId > 0)
            {
                item["paperId"] = s.paperId;
            }
            list.push_back(item);
        }
        sendJson(res, 200, list);
    });

    server_.Get(R"(/api/papers/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        int id = stoi(req.matches[1]);
        int userId = currentUserId(req);
        optional<json> paper = papers_.details(id, userId);
        if (!paper)
        {
            sendError(res, 404, "Paper not found");
            return;
        }
        onPaperViewed(userId, id);
        sendJson(res, 200, *paper);
    });

    server_.Get("/api/categories", [this](const httplib::Request&, httplib::Response& res) {
        sendJson(res, 200, papers_.categories());
    });

    server_.Get("/api/filters", [this](const httplib::Request&, httplib::Response& res) {
        sendJson(res, 200, papers_.filterOptions());
    });

    server_.Get("/api/featured", [this](const httplib::Request& req, httplib::Response& res) {
        string category = textParam(req, "category", 100);
        int limit = intParam(req, "limit", 6, 1, 24);
        sendJson(res, 200, papers_.featured(category, limit, currentUserId(req)));
    });
}
