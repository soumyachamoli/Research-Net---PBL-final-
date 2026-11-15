#include "HttpUtils.h"
#include <cmath>
using namespace std;
using json = nlohmann::json;

void sendJson(httplib::Response& res, int status, const json& body)
{
    res.status = status;
    res.set_content(body.dump(-1, ' ', false, json::error_handler_t::replace), "application/json; charset=utf-8");
}

void sendError(httplib::Response& res, int status, const string& message)
{
    sendJson(res, status, {{"error", message}});
}

string trim(const string& text)
{
    size_t start = text.find_first_not_of(" \t\r\n");
    if (start == string::npos)
    {
        return "";
    }
    size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

string textParam(const httplib::Request& req, const char* name, size_t maxLength)
{
    string value = trim(req.get_param_value(name));
    if (value.size() > maxLength)
    {
        throw BadRequest(string(name) + " is too long (max " + to_string(maxLength) + " characters)");
    }
    return value;
}

int intParam(const httplib::Request& req, const char* name, int fallback, int min, int max)
{
    string value = trim(req.get_param_value(name));
    if (value.empty())
    {
        return fallback;
    }
    size_t used = 0;
    long number = 0;
    try
    {
        number = stol(value, &used);
    }
    catch (...)
    {
        used = 0;
    }
    if (used != value.size() || number < min || number > max)
    {
        throw BadRequest(string(name) + " must be a whole number from " + to_string(min) + " to " + to_string(max));
    }
    return (int)number;
}

optional<double> numberParam(const httplib::Request& req, const char* name, double min, double max)
{
    string value = trim(req.get_param_value(name));
    if (value.empty())
    {
        return nullopt;
    }
    size_t used = 0;
    double number = 0;
    try
    {
        number = stod(value, &used);
    }
    catch (...)
    {
        used = 0;
    }
    if (used != value.size() || !isfinite(number) || number < min || number > max)
    {
        throw BadRequest(string(name) + " must be a number from " + to_string((int)min) + " to " + to_string((int)max));
    }
    return number;
}

json jsonBody(const httplib::Request& req)
{
    json body = json::parse(req.body, nullptr, false);
    if (body.is_discarded() || !body.is_object())
    {
        throw BadRequest("Request body must be a JSON object");
    }
    return body;
}

string jsonText(const json& body, const char* field, size_t maxLength)
{
    if (!body.contains(field) || body[field].is_null())
    {
        return "";
    }
    if (!body[field].is_string())
    {
        throw BadRequest(string(field) + " must be text");
    }
    string value = body[field].get<string>();
    if (value.size() > maxLength)
    {
        throw BadRequest(string(field) + " is too long (max " + to_string(maxLength) + " characters)");
    }
    return value;
}
