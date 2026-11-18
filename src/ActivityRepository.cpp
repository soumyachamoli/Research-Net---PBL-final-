#include "ActivityRepository.h"
using namespace std;
using json = nlohmann::json;

ActivityRepository::ActivityRepository(Database& db) : db_(db)
{
}

bool ActivityRepository::addBookmark(int userId, int paperId)
{
    Result r = db_.query(R"(
        INSERT INTO bookmarks (user_id, paper_id)
        SELECT $1, id FROM papers WHERE id = $2
        ON CONFLICT DO NOTHING
        RETURNING paper_id)",
        {toParam(userId), toParam(paperId)});
    if (r.rows() > 0)
    {
        return true;
    }
    Result exists = db_.query("SELECT 1 FROM papers WHERE id = $1", {toParam(paperId)});
    return exists.rows() > 0;
}

bool ActivityRepository::removeBookmark(int userId, int paperId)
{
    Result r = db_.query("DELETE FROM bookmarks WHERE user_id = $1 AND paper_id = $2 RETURNING paper_id",
                         {toParam(userId), toParam(paperId)});
    return r.rows() > 0;
}

vector<pair<int, string>> ActivityRepository::bookmarks(int userId, int limit)
{
    Result r = db_.query("SELECT paper_id, created_at FROM bookmarks WHERE user_id = $1 "
                         "ORDER BY created_at DESC, paper_id LIMIT $2",
                         {toParam(userId), toParam(limit)});
    vector<pair<int, string>> list;
    for (int i = 0; i < r.rows(); i++)
    {
        list.push_back({r.integer(i, "paper_id"), r.text(i, "created_at")});
    }
    return list;
}

void ActivityRepository::recordSearch(int userId, const string& query, const json& filters, long resultCount)
{
    db_.query(R"(
        INSERT INTO search_history (user_id, query, filters, result_count)
        SELECT $1, $2, $3::jsonb, $4
         WHERE NOT EXISTS (SELECT 1 FROM search_history
                            WHERE user_id = $1 AND query = $2 AND filters = $3::jsonb
                              AND created_at > now() - interval '10 minutes'))",
        {toParam(userId), query, filters.dump(), to_string(resultCount)});
}

json ActivityRepository::searches(int userId, int limit)
{
    Result r = db_.query("SELECT id, query, filters, result_count, created_at FROM search_history "
                         "WHERE user_id = $1 ORDER BY created_at DESC, id DESC LIMIT $2",
                         {toParam(userId), toParam(limit)});
    json list = json::array();
    for (int i = 0; i < r.rows(); i++)
    {
        list.push_back({
            {"id", stol(r.text(i, "id"))},
            {"query", r.text(i, "query")},
            {"filters", json::parse(r.text(i, "filters"))},
            {"resultCount", r.integer(i, "result_count")},
            {"searchedAt", r.text(i, "created_at")},
        });
    }
    return list;
}

vector<string> ActivityRepository::recentQueries(int userId, int limit)
{
    Result r = db_.query(R"(
        SELECT query FROM search_history WHERE user_id = $1 AND query <> ''
         GROUP BY query ORDER BY max(created_at) DESC LIMIT $2)",
        {toParam(userId), toParam(limit)});
    vector<string> list;
    for (int i = 0; i < r.rows(); i++)
    {
        list.push_back(r.text(i, "query"));
    }
    return list;
}

bool ActivityRepository::deleteSearch(int userId, long searchId)
{
    Result r = db_.query("DELETE FROM search_history WHERE user_id = $1 AND id = $2 RETURNING id",
                         {toParam(userId), to_string(searchId)});
    return r.rows() > 0;
}

void ActivityRepository::recordView(int userId, int paperId)
{
    db_.query(R"(
        INSERT INTO view_history (user_id, paper_id) VALUES ($1, $2)
        ON CONFLICT (user_id, paper_id) DO UPDATE SET viewed_at = now())",
        {toParam(userId), toParam(paperId)});
}

vector<pair<int, string>> ActivityRepository::views(int userId, int limit)
{
    Result r = db_.query("SELECT paper_id, viewed_at FROM view_history WHERE user_id = $1 "
                         "ORDER BY viewed_at DESC, paper_id LIMIT $2",
                         {toParam(userId), toParam(limit)});
    vector<pair<int, string>> list;
    for (int i = 0; i < r.rows(); i++)
    {
        list.push_back({r.integer(i, "paper_id"), r.text(i, "viewed_at")});
    }
    return list;
}

bool ActivityRepository::deleteView(int userId, int paperId)
{
    Result r = db_.query("DELETE FROM view_history WHERE user_id = $1 AND paper_id = $2 RETURNING paper_id",
                         {toParam(userId), toParam(paperId)});
    return r.rows() > 0;
}

void ActivityRepository::clearHistory(int userId)
{
    db_.transaction([&](Transaction& tx) {
        tx.query("DELETE FROM search_history WHERE user_id = $1", {toParam(userId)});
        tx.query("DELETE FROM view_history WHERE user_id = $1", {toParam(userId)});
    });
}

long ActivityRepository::deleteHistoryOlderThan(int days)
{
    long removed = 0;
    db_.transaction([&](Transaction& tx) {
        Param age = toParam(days);
        removed += tx.query("DELETE FROM search_history WHERE created_at < now() - make_interval(days => $1)", {age}).affected();
        removed += tx.query("DELETE FROM view_history WHERE viewed_at < now() - make_interval(days => $1)", {age}).affected();
    });
    return removed;
}

vector<string> ActivityRepository::interests(int userId)
{
    Result r = db_.query("SELECT interest FROM user_interests WHERE user_id = $1 ORDER BY interest",
                         {toParam(userId)});
    vector<string> list;
    for (int i = 0; i < r.rows(); i++)
    {
        list.push_back(r.text(i, "interest"));
    }
    return list;
}

void ActivityRepository::setInterests(int userId, const vector<string>& interests)
{
    json list = interests;
    db_.transaction([&](Transaction& tx) {
        tx.query("DELETE FROM user_interests WHERE user_id = $1", {toParam(userId)});
        tx.query("INSERT INTO user_interests (user_id, interest) "
                 "SELECT DISTINCT $1::int, value FROM json_array_elements_text($2::json) AS value "
                 "ON CONFLICT DO NOTHING",
                 {toParam(userId), list.dump()});
    });
}

json ActivityRepository::counts(int userId)
{
    Result r = db_.query(R"(
        SELECT (SELECT count(*) FROM bookmarks WHERE user_id = $1) AS bookmarks,
               (SELECT count(*) FROM search_history WHERE user_id = $1) AS searches,
               (SELECT count(*) FROM view_history WHERE user_id = $1) AS views)",
        {toParam(userId)});
    return {{"bookmarks", r.integer(0, "bookmarks")}, {"searches", r.integer(0, "searches")},
            {"views", r.integer(0, "views")}};
}
