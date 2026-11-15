#ifndef HTTPUTILS_H
#define HTTPUTILS_H

#include "httplib.h"
#include "json.hpp"
#include <optional>
#include <stdexcept>
#include <string>

// Thrown for invalid user input; the server turns it into "400 Bad Request".
class BadRequest : public std::runtime_error
{
public:
    explicit BadRequest(const std::string& message) : std::runtime_error(message) {}
};

void sendJson(httplib::Response& res, int status, const nlohmann::json& body);
void sendError(httplib::Response& res, int status, const std::string& message);

// Query-string parameters (?page=2&q=...). Each throws BadRequest when the value is not valid.
std::string textParam(const httplib::Request& req, const char* name, size_t maxLength = 200);
int intParam(const httplib::Request& req, const char* name, int fallback, int min, int max);
std::optional<double> numberParam(const httplib::Request& req, const char* name, double min, double max);

// The JSON body of a POST/PUT request (an object), or BadRequest.
nlohmann::json jsonBody(const httplib::Request& req);
std::string jsonText(const nlohmann::json& body, const char* field, size_t maxLength);

std::string trim(const std::string& text);

#endif
