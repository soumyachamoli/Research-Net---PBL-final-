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

class WebServer
{
public:
    WebServer(const Config& config, Database& db, EngineHolder& engines);
    bool start();

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

    void post(const std::string& pattern, httplib::Server::Handler handler);
    void put(const std::string& pattern, httplib::Server::Handler handler);
    void del(const std::string& pattern, httplib::Server::Handler handler);

    std::optional<User> currentUser(const httplib::Request& req);
    int currentUserId(const httplib::Request& req);
    std::optional<User> requireUser(const httplib::Request& req, httplib::Response& res);
    std::string clientAddress(const httplib::Request& req) const;
    void setSessionCookie(httplib::Response& res, const std::string& token, int maxAgeSeconds);

    void onSearch(int userId, const std::string& query, const PaperFilter& filter, long resultCount);
    void onPaperViewed(int userId, int paperId);
};

#endif
