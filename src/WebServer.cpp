#include "WebServer.h"
#include "HttpUtils.h"
#include <iostream>
using namespace std;
using json = nlohmann::json;

WebServer::WebServer(const Config& config, Database& db, EngineHolder& engines)
    : config_(config), db_(db), engines_(engines), papers_(db), users_(db)
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

    // Protection against cross-site requests: a request that changes data (POST, PUT,
    // DELETE) must come from our own pages. Browsers always send Origin on those, and the
    // session cookie is SameSite=Lax as a second layer.
    server_.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        if (req.method == "GET" || req.method == "HEAD")
        {
            return httplib::Server::HandlerResponse::Unhandled;
        }
        string origin = req.get_header_value("Origin");
        if (!origin.empty())
        {
            string host = req.get_header_value("X-Forwarded-Host");
            if (host.empty())
            {
                host = req.get_header_value("Host");
            }
            size_t scheme = origin.find("://");
            if (scheme == string::npos || origin.substr(scheme + 3) != host)
            {
                sendError(res, 403, "Cross-site request blocked");
                return httplib::Server::HandlerResponse::Handled;
            }
        }
        if ((req.method == "POST" || req.method == "PUT") && !req.body.empty() &&
            req.get_header_value("Content-Type").find("application/json") != 0)
        {
            sendError(res, 415, "Send JSON (Content-Type: application/json)");
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    registerPaperRoutes();
    registerAuthRoutes();
}

// ------------------------------------------------------------ sessions

static const char* SESSION_COOKIE = "rn_session";

// Finds rn_session=<64 hex characters> in the Cookie header.
static string sessionToken(const httplib::Request& req)
{
    string cookies = req.get_header_value("Cookie");
    string key = string(SESSION_COOKIE) + "=";
    size_t start = 0;
    while (start < cookies.size())
    {
        size_t end = cookies.find(';', start);
        string part = trim(cookies.substr(start, end == string::npos ? string::npos : end - start));
        if (part.rfind(key, 0) == 0)
        {
            string token = part.substr(key.size());
            if (token.size() == 64 && token.find_first_not_of("0123456789abcdef") == string::npos)
            {
                return token;
            }
        }
        if (end == string::npos)
        {
            break;
        }
        start = end + 1;
    }
    return "";
}

optional<User> WebServer::currentUser(const httplib::Request& req)
{
    string token = sessionToken(req);
    if (token.empty())
    {
        return nullopt;
    }
    return users_.userForSession(token);
}

int WebServer::currentUserId(const httplib::Request& req)
{
    optional<User> user = currentUser(req);
    return user ? user->id : 0;
}

optional<User> WebServer::requireUser(const httplib::Request& req, httplib::Response& res)
{
    optional<User> user = currentUser(req);
    if (!user)
    {
        sendError(res, 401, "Please log in");
    }
    return user;
}

string WebServer::clientAddress(const httplib::Request& req) const
{
    if (config_.trustProxy)
    {
        string forwarded = req.get_header_value("X-Forwarded-For");
        if (!forwarded.empty())
        {
            return trim(forwarded.substr(0, forwarded.find(',')));   // the original client
        }
    }
    return req.remote_addr;
}

// HttpOnly: JavaScript cannot read the cookie. SameSite=Lax: other sites cannot send it
// with their requests. Secure (production): only over HTTPS.
void WebServer::setSessionCookie(httplib::Response& res, const string& token, int maxAgeSeconds)
{
    string cookie = string(SESSION_COOKIE) + "=" + token + "; Path=/; HttpOnly; SameSite=Lax; Max-Age=" +
                    to_string(maxAgeSeconds);
    if (config_.cookieSecure)
    {
        cookie += "; Secure";
    }
    res.set_header("Set-Cookie", cookie);
}

// ------------------------------------------------------------ login limiter

static const size_t MAX_FAILED_LOGINS = 10;
static const auto LOGIN_WINDOW = chrono::minutes(15);

void LoginLimiter::forgetOld(deque<chrono::steady_clock::time_point>& times)
{
    auto now = chrono::steady_clock::now();
    while (!times.empty() && now - times.front() > LOGIN_WINDOW)
    {
        times.pop_front();
    }
}

bool LoginLimiter::blocked(const string& client)
{
    lock_guard<mutex> lock(mutex_);
    auto& times = failures_[client];
    forgetOld(times);
    return times.size() >= MAX_FAILED_LOGINS;
}

void LoginLimiter::recordFailure(const string& client)
{
    lock_guard<mutex> lock(mutex_);
    auto& times = failures_[client];
    forgetOld(times);
    times.push_back(chrono::steady_clock::now());
}

void LoginLimiter::reset(const string& client)
{
    lock_guard<mutex> lock(mutex_);
    failures_.erase(client);
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
