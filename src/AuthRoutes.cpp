#include "HttpUtils.h"
#include "WebServer.h"
#include <regex>
using namespace std;
using json = nlohmann::json;

static const int SESSION_DAYS = 7;

static json userJson(const User& user)
{
    return {{"id", user.id}, {"name", user.name}, {"email", user.email}};
}

static string normalizeEmail(const string& email)
{
    string result = trim(email);
    for (char& c : result)
    {
        if (c >= 'A' && c <= 'Z')
        {
            c = c - 'A' + 'a';
        }
    }
    return result;
}

static bool isValidEmail(const string& email)
{
    static const regex pattern(R"(^[^@\s]+@[^@\s]+\.[^@\s]+$)");
    return email.size() <= 254 && regex_match(email, pattern);
}

static void checkPassword(const string& password)
{
    if (password.size() < 8)
    {
        throw BadRequest("Password must be at least 8 characters");
    }
    if (password.size() > 72)
    {
        throw BadRequest("Password must be at most 72 characters");
    }
}

static string checkName(const string& rawName)
{
    string name = trim(rawName);
    if (name.empty())
    {
        throw BadRequest("Please enter your full name");
    }
    if (name.size() > 100)
    {
        throw BadRequest("Name is too long (max 100 characters)");
    }
    return name;
}

void WebServer::registerAuthRoutes()
{
    post("/api/register", [this](const httplib::Request& req, httplib::Response& res) {
        json body = jsonBody(req);
        string name = checkName(jsonText(body, "name", 200));
        string email = normalizeEmail(jsonText(body, "email", 300));
        string password = jsonText(body, "password", 200);
        if (!isValidEmail(email))
        {
            throw BadRequest("Please enter a valid email address");
        }
        checkPassword(password);
        if (body.contains("confirmPassword") && jsonText(body, "confirmPassword", 200) != password)
        {
            throw BadRequest("Passwords do not match");
        }

        optional<User> user = users_.create(name, email, password);
        if (!user)
        {
            sendError(res, 409, "An account with this email already exists");
            return;
        }
        setSessionCookie(res, users_.createSession(user->id, SESSION_DAYS), SESSION_DAYS * 86400);
        sendJson(res, 201, {{"user", userJson(*user)}});
    });

    post("/api/login", [this](const httplib::Request& req, httplib::Response& res) {
        string client = clientAddress(req);
        if (loginLimiter_.blocked(client))
        {
            sendError(res, 429, "Too many failed logins. Please wait 15 minutes and try again.");
            return;
        }
        json body = jsonBody(req);
        string email = normalizeEmail(jsonText(body, "email", 300));
        string password = jsonText(body, "password", 200);
        if (email.empty() || password.empty())
        {
            throw BadRequest("Please enter your email and password");
        }

        optional<User> user = users_.verifyPassword(email, password);
        if (!user)
        {
            loginLimiter_.recordFailure(client);
            sendError(res, 401, "Wrong email or password");
            return;
        }
        loginLimiter_.reset(client);
        setSessionCookie(res, users_.createSession(user->id, SESSION_DAYS), SESSION_DAYS * 86400);
        sendJson(res, 200, {{"user", userJson(*user)}});
    });

    post("/api/logout", [this](const httplib::Request& req, httplib::Response& res) {
        string cookies = req.get_header_value("Cookie");
        size_t at = cookies.find("rn_session=");
        if (at != string::npos)
        {
            users_.deleteSession(cookies.substr(at + 11, 64));
        }
        setSessionCookie(res, "", 0);
        sendJson(res, 200, {{"ok", true}});
    });

    server_.Get("/api/me", [this](const httplib::Request& req, httplib::Response& res) {
        optional<User> user = requireUser(req, res);
        if (user)
        {
            sendJson(res, 200, {{"user", userJson(*user)}});
        }
    });
}
