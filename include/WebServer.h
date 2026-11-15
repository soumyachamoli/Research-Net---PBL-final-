#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "Config.h"
#include "ActivityRepository.h"
#include "Database.h"
#include "EngineHolder.h"
#include "PaperRepository.h"
#include "UserRepository.h"
#include "httplib.h"
#include <chrono>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

// Counts failed logins per client; too many in 15 minutes -> temporarily blocked.
class LoginLimiter
{
public:
    bool blocked(const std::string& client);
    void recordFailure(const std::string& client);
    void reset(const std::string& client);

private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::deque<std::chrono::steady_clock::time_point>> failures_;
    void forgetOld(std::deque<std::chrono::steady_clock::time_point>& times);
};

// The REST API and the static website (web/ folder), on top of cpp-httplib.
// Route handlers are grouped by feature:
//   PaperRoutes.cpp  search, filters, autocomplete, paper details
//   AuthRoutes.cpp   register, login, logout, current user
//   UserRoutes.cpp   bookmarks, history, profile, recommendations, similar papers
class WebServer
{
public:
    WebServer(const Config& config, Database& db, EngineHolder& engines);
    bool start();     // blocks while the server runs; false if the port cannot be opened

private:
    const Config& config_;
    Database& db_;
    EngineHolder& engines_;
    PaperRepository papers_;
    UserRepository users_;
    ActivityRepository activity_;
    LoginLimiter loginLimiter_;
    httplib::Server server_;

    void registerPaperRoutes();
    void registerAuthRoutes();
    void registerUserRoutes();

    // Register a route that changes data; cross-site and non-JSON requests are rejected first.
    void post(const std::string& pattern, httplib::Server::Handler handler);
    void put(const std::string& pattern, httplib::Server::Handler handler);
    void del(const std::string& pattern, httplib::Server::Handler handler);

    // Logged-in user of a request (from the session cookie), if any.
    std::optional<User> currentUser(const httplib::Request& req);
    int currentUserId(const httplib::Request& req);
    // Like currentUser, but answers "401 Please log in" when nobody is logged in.
    std::optional<User> requireUser(const httplib::Request& req, httplib::Response& res);
    std::string clientAddress(const httplib::Request& req) const;
    void setSessionCookie(httplib::Response& res, const std::string& token, int maxAgeSeconds);

    // Called after a search / when a paper is opened (records history for logged-in users).
    void onSearch(int userId, const std::string& query, const PaperFilter& filter, long resultCount);
    void onPaperViewed(int userId, int paperId);
};

#endif
