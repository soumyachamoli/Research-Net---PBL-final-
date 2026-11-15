#ifndef ACTIVITYREPOSITORY_H
#define ACTIVITYREPOSITORY_H

#include "Database.h"
#include "json.hpp"
#include <string>
#include <utility>
#include <vector>

class ActivityRepository
{
public:
    explicit ActivityRepository(Database& db);

    bool addBookmark(int userId, int paperId);
    bool removeBookmark(int userId, int paperId);
    std::vector<std::pair<int, std::string>> bookmarks(int userId, int limit);

    void recordSearch(int userId, const std::string& query, const nlohmann::json& filters, long resultCount);
    nlohmann::json searches(int userId, int limit);
    std::vector<std::string> recentQueries(int userId, int limit);
    bool deleteSearch(int userId, long searchId);

    void recordView(int userId, int paperId);
    std::vector<std::pair<int, std::string>> views(int userId, int limit);
    bool deleteView(int userId, int paperId);

    void clearHistory(int userId);

    std::vector<std::string> interests(int userId);
    void setInterests(int userId, const std::vector<std::string>& interests);

    nlohmann::json counts(int userId);

private:
    Database& db_;
};

#endif
