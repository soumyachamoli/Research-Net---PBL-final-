#ifndef ACTIVITYREPOSITORY_H
#define ACTIVITYREPOSITORY_H

#include "Database.h"
#include "json.hpp"
#include <string>
#include <utility>
#include <vector>

// A user's own data: bookmarks, search history, viewed papers and research interests.
// Every query has "WHERE user_id = $1", with the id taken from the login session, so one
// user can never read or change another user's data.
class ActivityRepository
{
public:
    explicit ActivityRepository(Database& db);

    // Bookmarks. addBookmark returns false if the paper does not exist.
    bool addBookmark(int userId, int paperId);
    bool removeBookmark(int userId, int paperId);
    std::vector<std::pair<int, std::string>> bookmarks(int userId, int limit);   // (paper id, saved at)

    // Search history. The same search repeated within 10 minutes is stored once.
    void recordSearch(int userId, const std::string& query, const nlohmann::json& filters, long resultCount);
    nlohmann::json searches(int userId, int limit);
    std::vector<std::string> recentQueries(int userId, int limit);
    bool deleteSearch(int userId, long searchId);

    // Papers the user opened (one entry per paper, newest view first).
    void recordView(int userId, int paperId);
    std::vector<std::pair<int, std::string>> views(int userId, int limit);       // (paper id, viewed at)
    bool deleteView(int userId, int paperId);

    void clearHistory(int userId);

    std::vector<std::string> interests(int userId);
    void setInterests(int userId, const std::vector<std::string>& interests);

    nlohmann::json counts(int userId);

private:
    Database& db_;
};

#endif
