#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "Database.h"
#include "SearchEngine.h"

// A very small web server. It answers ONE browser request at a time (no threads):
// files from the web/ folder for the page, and JSON for /api/... requests.
class WebServer
{
private:
    SearchEngine& engine;
    Database& db;

public:
    WebServer(SearchEngine& searchEngine, Database& database);
    void start(int port);
};

#endif
