#include "PaperRepository.h"
#include <unordered_map>
using namespace std;
using json = nlohmann::json;

// Columns of a paper card. {USER} is replaced with the parameter holding the user id
// (0 when nobody is logged in, so "bookmarked" is simply false).
static const string SUMMARY_COLUMNS = R"(
    p.id, p.title, left(p.abstract, 320) AS preview, p.publication_year, p.publication_date,
    p.publication_type, p.doi, p.url, p.cited_by_count, p.category, p.topic,
    j.name AS journal, j.impact_factor, j.impact_factor_source, j.impact_factor_year,
    (SELECT json_agg(a.name ORDER BY pa.position)
       FROM (SELECT author_id, position FROM paper_authors
              WHERE paper_id = p.id ORDER BY position LIMIT 6) pa
       JOIN authors a ON a.id = pa.author_id) AS authors,
    (SELECT count(*) FROM paper_authors WHERE paper_id = p.id) AS author_count,
    EXISTS (SELECT 1 FROM bookmarks b WHERE b.user_id = {USER} AND b.paper_id = p.id) AS bookmarked)";

static string withUser(string sql, const string& placeholder)
{
    size_t at = sql.find("{USER}");
    sql.replace(at, 6, placeholder);
    return sql;
}

static json optionalNumber(const Result& r, int row, const string& column)
{
    optional<double> value = r.number(row, column);
    return value ? json(*value) : json(nullptr);
}

static json optionalText(const Result& r, int row, const string& column)
{
    return r.isNull(row, column) ? json(nullptr) : json(r.text(row, column));
}

json paperSummaryJson(const Result& r, int row)
{
    json paper = {
        {"id", r.integer(row, "id")},
        {"title", r.text(row, "title")},
        {"abstractPreview", r.text(row, "preview")},
        {"year", r.isNull(row, "publication_year") ? json(nullptr) : json(r.integer(row, "publication_year"))},
        {"publicationDate", optionalText(r, row, "publication_date")},
        {"type", optionalText(r, row, "publication_type")},
        {"doi", optionalText(r, row, "doi")},
        {"url", optionalText(r, row, "url")},
        {"citedBy", r.integer(row, "cited_by_count")},
        {"category", optionalText(r, row, "category")},
        {"topic", optionalText(r, row, "topic")},
        {"journal", optionalText(r, row, "journal")},
        {"impactFactor", optionalNumber(r, row, "impact_factor")},
        {"impactFactorSource", optionalText(r, row, "impact_factor_source")},
        {"impactFactorYear", r.isNull(row, "impact_factor_year") ? json(nullptr) : json(r.integer(row, "impact_factor_year"))},
        {"authors", r.isNull(row, "authors") ? json::array() : json::parse(r.text(row, "authors"))},
        {"authorCount", r.integer(row, "author_count")},
        {"bookmarked", r.boolean(row, "bookmarked")},
    };
    return paper;
}

bool PaperFilter::hasAnyFilter() const
{
    return !author.empty() || !journal.empty() || !category.empty() || !keyword.empty() || !type.empty() ||
           year > 0 || yearFrom > 0 || yearTo > 0 || minImpact.has_value() || maxImpact.has_value();
}

json PaperFilter::toJson() const
{
    json j = json::object();
    if (!author.empty()) j["author"] = author;
    if (!journal.empty()) j["journal"] = journal;
    if (!category.empty()) j["category"] = category;
    if (!keyword.empty()) j["keyword"] = keyword;
    if (!type.empty()) j["type"] = type;
    if (year > 0) j["year"] = year;
    if (yearFrom > 0) j["year_from"] = yearFrom;
    if (yearTo > 0) j["year_to"] = yearTo;
    if (minImpact) j["min_impact"] = *minImpact;
    if (maxImpact) j["max_impact"] = *maxImpact;
    if (sort != "relevance") j["sort"] = sort;
    return j;
}

bool isValidSort(const string& sort)
{
    return sort == "relevance" || sort == "newest" || sort == "oldest" || sort == "impact_desc" ||
           sort == "impact_asc" || sort == "citations" || sort == "author";
}

// "50%_off" -> "50\%\_off": the user's text is matched literally inside ILIKE '%...%'.
static string escapeLike(const string& text)
{
    string escaped;
    for (char c : text)
    {
        if (c == '%' || c == '_' || c == '\\')
        {
            escaped += '\\';
        }
        escaped += c;
    }
    return escaped;
}

PaperRepository::PaperRepository(Database& db) : db_(db)
{
}

vector<Paper> PaperRepository::loadAll()
{
    Result r = db_.query(R"(
        SELECT p.id, p.title, coalesce(p.abstract, '') AS abstract, coalesce(p.publication_year, 0) AS year,
               coalesce(p.category, '') AS category, coalesce(p.topic, '') AS topic,
               p.cited_by_count, coalesce(j.name, '') AS journal, j.impact_factor,
               (SELECT json_agg(json_build_array(a.id, a.name) ORDER BY pa.position)
                  FROM paper_authors pa JOIN authors a ON a.id = pa.author_id
                 WHERE pa.paper_id = p.id) AS authors,
               (SELECT json_agg(k.name ORDER BY k.name)
                  FROM paper_keywords pk JOIN keywords k ON k.id = pk.keyword_id
                 WHERE pk.paper_id = p.id) AS keywords
          FROM papers p LEFT JOIN journals j ON j.id = p.journal_id
         ORDER BY p.id)");

    vector<Paper> papers;
    papers.reserve(r.rows());
    for (int i = 0; i < r.rows(); i++)
    {
        Paper paper;
        paper.id = r.integer(i, "id");
        paper.title = r.text(i, "title");
        paper.abstractText = r.text(i, "abstract");
        paper.year = r.integer(i, "year");
        paper.category = r.text(i, "category");
        paper.topic = r.text(i, "topic");
        paper.citedBy = r.integer(i, "cited_by_count");
        paper.journal = r.text(i, "journal");
        paper.impactFactor = r.number(i, "impact_factor");
        if (!r.isNull(i, "authors"))
        {
            for (const json& author : json::parse(r.text(i, "authors")))
            {
                paper.authorIds.push_back(author[0].get<int>());
                paper.authors.push_back(author[1].get<string>());
            }
        }
        if (!r.isNull(i, "keywords"))
        {
            paper.keywords = json::parse(r.text(i, "keywords")).get<vector<string>>();
        }
        papers.push_back(std::move(paper));
    }
    return papers;
}

string PaperRepository::dataVersion()
{
    Result r = db_.query("SELECT count(*) || '/' || coalesce(max(updated_at)::text, '') AS version FROM papers");
    return r.text(0, "version");
}

json PaperRepository::search(const vector<SearchHit>* hits, const PaperFilter& filter, int userId)
{
    Params params;
    auto add = [&params](Param value) {
        params.push_back(std::move(value));
        return "$" + to_string(params.size());
    };

    // Step 1: where the candidate papers come from.
    string from;
    if (hits != nullptr)
    {
        vector<int> ids;
        vector<double> scores;
        for (const SearchHit& hit : *hits)
        {
            ids.push_back(hit.paperId);
            scores.push_back(hit.score);
        }
        from = "FROM unnest(" + add(toArrayParam(ids)) + "::int[], " + add(toArrayParam(scores)) +
               "::float8[]) AS r(id, score) JOIN papers p ON p.id = r.id";
    }
    else
    {
        from = "FROM papers p CROSS JOIN (SELECT 0.0::float8 AS score) r";
    }
    from += " LEFT JOIN journals j ON j.id = p.journal_id";

    // Step 2: every filter the user set becomes one condition (all combined with AND).
    string where = " WHERE true";
    if (!filter.author.empty())
    {
        // Matches the start of any word of the name: "he" finds "Kaiming He" and "Hector
        // Garcia", but not "Schwarz". Both patterns can use the trigram index.
        string name = add(escapeLike(filter.author));
        where += " AND EXISTS (SELECT 1 FROM paper_authors pa JOIN authors a ON a.id = pa.author_id"
                 " WHERE pa.paper_id = p.id AND (a.name ILIKE " + name + " || '%' OR a.name ILIKE '% ' || " +
                 name + " || '%'))";
    }
    if (!filter.journal.empty())
    {
        where += " AND j.name ILIKE '%' || " + add(escapeLike(filter.journal)) + " || '%'";
    }
    if (!filter.category.empty())
    {
        where += " AND p.category = " + add(filter.category);
    }
    if (!filter.keyword.empty())
    {
        where += " AND EXISTS (SELECT 1 FROM paper_keywords pk JOIN keywords k ON k.id = pk.keyword_id"
                 " WHERE pk.paper_id = p.id AND lower(k.name) = lower(" + add(filter.keyword) + "))";
    }
    if (!filter.type.empty())
    {
        where += " AND p.publication_type = " + add(filter.type);
    }
    if (filter.year > 0)
    {
        where += " AND p.publication_year = " + add(toParam(filter.year));
    }
    if (filter.yearFrom > 0)
    {
        where += " AND p.publication_year >= " + add(toParam(filter.yearFrom));
    }
    if (filter.yearTo > 0)
    {
        where += " AND p.publication_year <= " + add(toParam(filter.yearTo));
    }
    // Papers without a known impact factor are excluded by an impact filter (NULL fails
    // the comparison) - they are not treated as having impact 0.
    if (filter.minImpact)
    {
        where += " AND j.impact_factor >= " + add(toParam(*filter.minImpact));
    }
    if (filter.maxImpact)
    {
        where += " AND j.impact_factor <= " + add(toParam(*filter.maxImpact));
    }

    // Step 3: sorting. Every order ends with p.id so pages never overlap.
    string order;
    if (filter.sort == "newest")
        order = "p.publication_date DESC NULLS LAST, p.publication_year DESC NULLS LAST, p.id";
    else if (filter.sort == "oldest")
        order = "p.publication_date ASC NULLS LAST, p.publication_year ASC NULLS LAST, p.id";
    else if (filter.sort == "impact_desc")
        order = "j.impact_factor DESC NULLS LAST, r.score DESC, p.id";
    else if (filter.sort == "impact_asc")
        order = "j.impact_factor ASC NULLS LAST, r.score DESC, p.id";
    else if (filter.sort == "citations")
        order = "p.cited_by_count DESC, p.id";
    else if (filter.sort == "author")
        order = "(SELECT a.name FROM paper_authors pa JOIN authors a ON a.id = pa.author_id"
                " WHERE pa.paper_id = p.id ORDER BY pa.position LIMIT 1) ASC NULLS LAST, p.id";
    else if (hits != nullptr)
        order = "r.score DESC, p.cited_by_count DESC, p.id";          // relevance
    else
        order = "p.cited_by_count DESC, p.id";                         // no query: most cited first

    // Step 4: one page of ids (the total comes from a window function in the same query),
    // then the card details for just those rows.
    int offset = (filter.page - 1) * filter.pageSize;
    string inner = "SELECT p.id, r.score, count(*) OVER () AS total, row_number() OVER (ORDER BY " + order +
                   ") AS ord " + from + where + " ORDER BY " + order + " LIMIT " + add(toParam(filter.pageSize)) +
                   " OFFSET " + add(toParam(offset));
    string user = add(toParam(userId));
    string sql = "SELECT s.score, s.total, " + withUser(SUMMARY_COLUMNS, user) + " FROM (" + inner +
                 ") s JOIN papers p ON p.id = s.id LEFT JOIN journals j ON j.id = p.journal_id ORDER BY s.ord";

    Result r = db_.query(sql, params);
    json results = json::array();
    long total = 0;
    for (int i = 0; i < r.rows(); i++)
    {
        json paper = paperSummaryJson(r, i);
        if (hits != nullptr)
        {
            paper["relevance"] = *r.number(i, "score");
        }
        results.push_back(paper);
        total = r.integer(i, "total");
    }
    return {{"total", total}, {"page", filter.page}, {"pageSize", filter.pageSize}, {"results", results}};
}

optional<json> PaperRepository::details(int paperId, int userId)
{
    Result r = db_.query(R"(
        SELECT p.id, p.title, p.abstract, p.publication_year, p.publication_date, p.publication_type,
               p.doi, p.url, p.cited_by_count, p.category, p.topic, p.data_source, p.retrieved_at,
               j.name AS journal, j.issn_l, j.publisher, j.source_type, j.impact_factor,
               j.impact_factor_source, j.impact_factor_year,
               (SELECT json_agg(json_build_object('id', a.id, 'name', a.name) ORDER BY pa.position)
                  FROM paper_authors pa JOIN authors a ON a.id = pa.author_id WHERE pa.paper_id = p.id) AS authors,
               (SELECT json_agg(k.name ORDER BY k.name)
                  FROM paper_keywords pk JOIN keywords k ON k.id = pk.keyword_id WHERE pk.paper_id = p.id) AS keywords,
               EXISTS (SELECT 1 FROM bookmarks b WHERE b.user_id = $2 AND b.paper_id = p.id) AS bookmarked
          FROM papers p LEFT JOIN journals j ON j.id = p.journal_id
         WHERE p.id = $1)",
        {toParam(paperId), toParam(userId)});
    if (r.rows() == 0)
    {
        return nullopt;
    }
    json journal = nullptr;
    if (!r.isNull(0, "journal"))
    {
        journal = {
            {"name", r.text(0, "journal")},
            {"issn", optionalText(r, 0, "issn_l")},
            {"publisher", optionalText(r, 0, "publisher")},
            {"type", optionalText(r, 0, "source_type")},
            {"impactFactor", optionalNumber(r, 0, "impact_factor")},
            {"impactFactorSource", optionalText(r, 0, "impact_factor_source")},
            {"impactFactorYear", r.isNull(0, "impact_factor_year") ? json(nullptr) : json(r.integer(0, "impact_factor_year"))},
        };
    }
    return json{
        {"id", r.integer(0, "id")},
        {"title", r.text(0, "title")},
        {"abstract", optionalText(r, 0, "abstract")},
        {"year", r.isNull(0, "publication_year") ? json(nullptr) : json(r.integer(0, "publication_year"))},
        {"publicationDate", optionalText(r, 0, "publication_date")},
        {"type", optionalText(r, 0, "publication_type")},
        {"doi", optionalText(r, 0, "doi")},
        {"url", optionalText(r, 0, "url")},
        {"citedBy", r.integer(0, "cited_by_count")},
        {"category", optionalText(r, 0, "category")},
        {"topic", optionalText(r, 0, "topic")},
        {"dataSource", r.text(0, "data_source")},
        {"retrievedAt", optionalText(r, 0, "retrieved_at")},
        {"journal", journal},
        {"authors", r.isNull(0, "authors") ? json::array() : json::parse(r.text(0, "authors"))},
        {"keywords", r.isNull(0, "keywords") ? json::array() : json::parse(r.text(0, "keywords"))},
        {"bookmarked", r.boolean(0, "bookmarked")},
    };
}

json PaperRepository::summaries(const vector<int>& ids, int userId)
{
    json list = json::array();
    if (ids.empty())
    {
        return list;
    }
    Result r = db_.query("SELECT " + withUser(SUMMARY_COLUMNS, "$2") +
                         " FROM papers p LEFT JOIN journals j ON j.id = p.journal_id WHERE p.id = ANY($1::int[])",
                         {toArrayParam(ids), toParam(userId)});
    unordered_map<int, json> byId;
    for (int i = 0; i < r.rows(); i++)
    {
        byId[r.integer(i, "id")] = paperSummaryJson(r, i);
    }
    for (int id : ids)                                  // keep the caller's order
    {
        auto found = byId.find(id);
        if (found != byId.end())
        {
            list.push_back(found->second);
        }
    }
    return list;
}

json PaperRepository::categories()
{
    Result r = db_.query("SELECT category, count(*) AS papers FROM papers WHERE category IS NOT NULL "
                         "GROUP BY category ORDER BY papers DESC, category");
    json list = json::array();
    for (int i = 0; i < r.rows(); i++)
    {
        list.push_back({{"name", r.text(i, "category")}, {"papers", r.integer(i, "papers")}});
    }
    return list;
}

json PaperRepository::filterOptions()
{
    Result years = db_.query("SELECT min(publication_year) AS lo, max(publication_year) AS hi FROM papers");
    Result impact = db_.query("SELECT min(j.impact_factor) AS lo, max(j.impact_factor) AS hi, "
                              "min(j.impact_factor_source) AS source, max(j.impact_factor_year) AS year "
                              "FROM journals j WHERE EXISTS (SELECT 1 FROM papers p WHERE p.journal_id = j.id)");
    Result types = db_.query("SELECT publication_type AS type, count(*) AS papers FROM papers "
                             "WHERE publication_type IS NOT NULL GROUP BY publication_type ORDER BY papers DESC");
    json typeList = json::array();
    for (int i = 0; i < types.rows(); i++)
    {
        typeList.push_back({{"name", types.text(i, "type")}, {"papers", types.integer(i, "papers")}});
    }
    return {
        {"years", {{"min", years.integer(0, "lo")}, {"max", years.integer(0, "hi")}}},
        {"impactFactor", {{"min", optionalNumber(impact, 0, "lo")}, {"max", optionalNumber(impact, 0, "hi")},
                          {"source", optionalText(impact, 0, "source")}, {"year", optionalText(impact, 0, "year")}}},
        {"categories", categories()},
        {"types", typeList},
        {"sorts", {"relevance", "newest", "oldest", "impact_desc", "impact_asc", "citations", "author"}},
    };
}

json PaperRepository::featured(const string& category, int limit, int userId)
{
    // Most cited papers of the last five years in the collection (optionally one category).
    Result r = db_.query("SELECT " + withUser(SUMMARY_COLUMNS, "$3") + R"(
          FROM papers p LEFT JOIN journals j ON j.id = p.journal_id
         WHERE p.publication_year >= (SELECT max(publication_year) - 4 FROM papers)
           AND ($1 = '' OR p.category = $1)
         ORDER BY p.cited_by_count DESC, p.id LIMIT $2)",
        {category, toParam(limit), toParam(userId)});
    json list = json::array();
    for (int i = 0; i < r.rows(); i++)
    {
        list.push_back(paperSummaryJson(r, i));
    }
    return list;
}
