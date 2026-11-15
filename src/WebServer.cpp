#include "WebServer.h"
#include "HttpUtils.h"
#include <iostream>
using namespace std;
using json = nlohmann::json;

WebServer::WebServer(const Config& config, Database& db, EngineHolder& engines)
    : config_(config), db_(db), engines_(engines), papers_(db)
{
    // The website: index.html, app.js, style.css from the web/ folder.
    if (!server_.set_mount_point("/", config_.webRoot))
    {
        cerr << "Warning: web folder '" << config_.webRoot << "' not found" << endl;
    }

    // Security headers on every response. The page only loads its own scripts and styles.
    server_.set_default_headers({
        {"X-Content-Type-Options", "nosniff"},
        {"Referrer-Policy", "strict-origin-when-cross-origin"},
        {"X-Frame-Options", "DENY"},
        {"Content-Security-Policy",
         "default-src 'self'; img-src 'self' data:; object-src 'none'; base-uri 'none'; frame-ancestors 'none'"},
    });

    // Any exception inside a handler ends up here instead of crashing the server.
    server_.set_exception_handler([](const httplib::Request& req, httplib::Response& res, exception_ptr error) {
        try
        {
            rethrow_exception(error);
        }
        catch (const BadRequest& e)
        {
            sendError(res, 400, e.what());
        }
        catch (const DatabaseError& e)
        {
            cerr << "Database error on " << req.method << " " << req.path << ": " << e.what() << endl;
            sendError(res, 500, "A database error occurred. Please try again.");
        }
        catch (const exception& e)
        {
            cerr << "Error on " << req.method << " " << req.path << ": " << e.what() << endl;
            sendError(res, 500, "Internal server error");
        }
    });

    // Unknown /api/ routes answer in JSON like the rest of the API.
    server_.set_error_handler([](const httplib::Request& req, httplib::Response& res) {
        if (req.path.rfind("/api/", 0) == 0 && res.body.empty())
        {
            sendError(res, res.status, res.status == 404 ? "Not found" : "Request failed");
        }
    });

    registerPaperRoutes();
}

int WebServer::currentUserId(const httplib::Request&)
{
    return 0;                                   // no accounts yet
}

void WebServer::onSearch(int, const string&, const PaperFilter&, long)
{
}

void WebServer::onPaperViewed(int, int)
{
}

bool WebServer::start()
{
    cout << "ResearchNet running at http://" << config_.host << ":" << config_.port << endl;
    if (!server_.listen(config_.host, config_.port))
    {
        cerr << "Could not listen on " << config_.host << ":" << config_.port << ". Is it already in use?" << endl;
        return false;
    }
    return true;
}
