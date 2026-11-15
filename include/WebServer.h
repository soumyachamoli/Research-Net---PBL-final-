#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "Config.h"
#include "Database.h"
#include "EngineHolder.h"
#include "PaperRepository.h"
#include "httplib.h"

// The REST API and the static website (web/ folder), on top of cpp-httplib.
// Route handlers are grouped by feature: PaperRoutes.cpp (search, papers, autocomplete).
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
    httplib::Server server_;

    void registerPaperRoutes();

    // Logged-in user of a request, 0 if none.
    int currentUserId(const httplib::Request& req);
    // Called after a search / when a paper is opened (records history for logged-in users).
    void onSearch(int userId, const std::string& query, const PaperFilter& filter, long resultCount);
    void onPaperViewed(int userId, int paperId);
};

#endif
