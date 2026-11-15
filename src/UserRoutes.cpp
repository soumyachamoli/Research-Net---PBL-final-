// Routes for a logged-in user's own data: bookmarks, history, profile, recommendations.
// Similar papers (/api/papers/:id/similar) are public.
#include "HttpUtils.h"
#include "Recommender.h"
#include "WebServer.h"
using namespace std;
using json = nlohmann::json;

// Paper cards + when they were saved/viewed, in the given order.
static json papersWithTime(PaperRepository& papers, const vector<pair<int, string>>& items, int userId,
                           const char* timeField)
{
    vector<int> ids;
    unordered_map<int, string> times;
    for (const auto& item : items)
    {
        ids.push_back(item.first);
        times[item.first] = item.second;
    }
    json list = papers.summaries(ids, userId);
    for (json& paper : list)
    {
        paper[timeField] = times[paper["id"].get<int>()];
    }
    return list;
}

// Recommendation results as JSON: the paper card, the score and how it was computed.
static json recommendationsJson(PaperRepository& papers, const vector<Recommendation>& recs, int userId)
{
    vector<int> ids;
    for (const Recommendation& rec : recs)
    {
        ids.push_back(rec.paperId);
    }
    json cards = papers.summaries(ids, userId);
    unordered_map<int, json> byId;
    for (const json& card : cards)
    {
        byId[card["id"].get<int>()] = card;
    }
    json list = json::array();
    for (const Recommendation& rec : recs)
    {
        if (!byId.count(rec.paperId))
        {
            continue;
        }
        list.push_back({
            {"paper", byId[rec.paperId]},
            {"score", rec.score},
            {"similarity", rec.similarity},
            {"components", {{"content", rec.content}, {"keywords", rec.keywords}, {"authors", rec.authors},
                            {"recency", rec.recency}, {"impact", rec.impact < 0 ? json(nullptr) : json(rec.impact)}}},
            {"reasons", rec.reasons},
        });
    }
    return list;
}

static vector<string> readInterests(const json& body)
{
    vector<string> interests;
    if (!body.contains("interests"))
    {
        return interests;
    }
    if (!body["interests"].is_array() || body["interests"].size() > 20)
    {
        throw BadRequest("interests must be a list of at most 20 items");
    }
    for (const json& item : body["interests"])
    {
        if (!item.is_string())
        {
            throw BadRequest("each interest must be text");
        }
        string interest = trim(item.get<string>());
        if (interest.size() > 60)
        {
            throw BadRequest("each interest can have at most 60 characters");
        }
        if (!interest.empty())
        {
            interests.push_back(interest);
        }
    }
    return interests;
}

void WebServer::onSearch(int userId, const string& query, const PaperFilter& filter, long resultCount)
{
    // Only real searches on the first page (not every page turn) go into the history.
    if (userId > 0 && filter.page == 1 && (!query.empty() || filter.hasAnyFilter()))
    {
        activity_.recordSearch(userId, query, filter.toJson(), resultCount);
    }
}

void WebServer::onPaperViewed(int userId, int paperId)
{
    if (userId > 0)
    {
        activity_.recordView(userId, paperId);
    }
}

void WebServer::registerUserRoutes()
{
    // ---------------------------------------------------------------- recommendations

    server_.Get(R"(/api/papers/(\d+)/similar)", [this](const httplib::Request& req, httplib::Response& res) {
        int paperId = stoi(req.matches[1]);
        int limit = intParam(req, "limit", 6, 1, 20);
        auto engine = engines_.get();
        if (engine->findPaper(paperId) == nullptr)
        {
            sendError(res, 404, "Paper not found");
            return;
        }
        Recommender recommender(*engine, config_.weights);
        sendJson(res, 200, recommendationsJson(papers_, recommender.similarTo(paperId, limit), currentUserId(req)));
    });

    server_.Get("/api/recommendations", [this](const httplib::Request& req, httplib::Response& res) {
        optional<User> user = requireUser(req, res);
        if (!user)
        {
            return;
        }
        int limit = intParam(req, "limit", 10, 1, 30);

        UserProfile profile;
        for (const auto& item : activity_.bookmarks(user->id, 50))
        {
            profile.savedPaperIds.push_back(item.first);
        }
        for (const auto& item : activity_.views(user->id, 30))
        {
            profile.viewedPaperIds.push_back(item.first);
        }
        profile.searches = activity_.recentQueries(user->id, 10);
        profile.interests = activity_.interests(user->id);

        auto engine = engines_.get();
        Recommender recommender(*engine, config_.weights);
        sendJson(res, 200, {
            {"basedOn", {{"saved", profile.savedPaperIds.size()}, {"viewed", profile.viewedPaperIds.size()},
                         {"searches", profile.searches.size()}, {"interests", profile.interests}}},
            {"results", recommendationsJson(papers_, recommender.forUser(profile, limit), user->id)},
        });
    });

    // ---------------------------------------------------------------- bookmarks

    server_.Get("/api/bookmarks", [this](const httplib::Request& req, httplib::Response& res) {
        optional<User> user = requireUser(req, res);
        if (user)
        {
            int limit = intParam(req, "limit", 200, 1, 500);
            sendJson(res, 200, papersWithTime(papers_, activity_.bookmarks(user->id, limit), user->id, "savedAt"));
        }
    });

    post("/api/bookmarks", [this](const httplib::Request& req, httplib::Response& res) {
        optional<User> user = requireUser(req, res);
        if (!user)
        {
            return;
        }
        json body = jsonBody(req);
        if (!body.contains("paperId") || !body["paperId"].is_number_integer())
        {
            throw BadRequest("paperId must be a number");
        }
        int paperId = body["paperId"].get<int>();
        if (!activity_.addBookmark(user->id, paperId))
        {
            sendError(res, 404, "Paper not found");
            return;
        }
        sendJson(res, 201, {{"paperId", paperId}, {"bookmarked", true}});
    });

    del(R"(/api/bookmarks/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        optional<User> user = requireUser(req, res);
        if (!user)
        {
            return;
        }
        int paperId = stoi(req.matches[1]);
        if (!activity_.removeBookmark(user->id, paperId))
        {
            sendError(res, 404, "This paper is not in your saved papers");
            return;
        }
        sendJson(res, 200, {{"paperId", paperId}, {"bookmarked", false}});
    });

    // ---------------------------------------------------------------- history

    server_.Get("/api/history", [this](const httplib::Request& req, httplib::Response& res) {
        optional<User> user = requireUser(req, res);
        if (user)
        {
            int limit = intParam(req, "limit", 50, 1, 200);
            sendJson(res, 200, {
                {"searches", activity_.searches(user->id, limit)},
                {"viewed", papersWithTime(papers_, activity_.views(user->id, limit), user->id, "viewedAt")},
            });
        }
    });

    del("/api/history", [this](const httplib::Request& req, httplib::Response& res) {
        optional<User> user = requireUser(req, res);
        if (user)
        {
            activity_.clearHistory(user->id);
            sendJson(res, 200, {{"ok", true}});
        }
    });

    del(R"(/api/history/searches/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        optional<User> user = requireUser(req, res);
        if (!user)
        {
            return;
        }
        if (!activity_.deleteSearch(user->id, stol(req.matches[1])))
        {
            sendError(res, 404, "Search not found");
            return;
        }
        sendJson(res, 200, {{"ok", true}});
    });

    del(R"(/api/history/views/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        optional<User> user = requireUser(req, res);
        if (!user)
        {
            return;
        }
        if (!activity_.deleteView(user->id, stoi(req.matches[1])))
        {
            sendError(res, 404, "Paper not found in your history");
            return;
        }
        sendJson(res, 200, {{"ok", true}});
    });

    // ---------------------------------------------------------------- profile

    server_.Get("/api/profile", [this](const httplib::Request& req, httplib::Response& res) {
        optional<User> user = requireUser(req, res);
        if (!user)
        {
            return;
        }
        Result r = db_.query("SELECT created_at FROM users WHERE id = $1", {toParam(user->id)});
        sendJson(res, 200, {
            {"user", {{"id", user->id}, {"name", user->name}, {"email", user->email}}},
            {"memberSince", r.rows() > 0 ? r.text(0, "created_at") : ""},
            {"interests", activity_.interests(user->id)},
            {"counts", activity_.counts(user->id)},
        });
    });

    // PUT /api/profile  {"name", "interests": [...]}  (both optional)
    put("/api/profile", [this](const httplib::Request& req, httplib::Response& res) {
        optional<User> user = requireUser(req, res);
        if (!user)
        {
            return;
        }
        json body = jsonBody(req);
        if (body.contains("name"))
        {
            string name = trim(jsonText(body, "name", 200));
            if (name.empty() || name.size() > 100)
            {
                throw BadRequest("Name must be 1 to 100 characters");
            }
            users_.updateName(user->id, name);
        }
        if (body.contains("interests"))
        {
            activity_.setInterests(user->id, readInterests(body));
        }
        optional<User> updated = users_.findById(user->id);
        sendJson(res, 200, {
            {"user", {{"id", updated->id}, {"name", updated->name}, {"email", updated->email}}},
            {"interests", activity_.interests(user->id)},
        });
    });

    // PUT /api/profile/password  {"currentPassword", "newPassword"}
    put("/api/profile/password", [this](const httplib::Request& req, httplib::Response& res) {
        optional<User> user = requireUser(req, res);
        if (!user)
        {
            return;
        }
        json body = jsonBody(req);
        string current = jsonText(body, "currentPassword", 200);
        string next = jsonText(body, "newPassword", 200);
        if (next.size() < 8 || next.size() > 72)
        {
            throw BadRequest("New password must be 8 to 72 characters");
        }
        if (!users_.changePassword(user->id, current, next))
        {
            sendError(res, 403, "Current password is wrong");
            return;
        }
        // All sessions were ended; give this browser a fresh one.
        setSessionCookie(res, users_.createSession(user->id, 7), 7 * 86400);
        sendJson(res, 200, {{"ok", true}});
    });
}
