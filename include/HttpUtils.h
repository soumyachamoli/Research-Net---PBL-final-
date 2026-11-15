#ifndef HTTPUTILS_H
#define HTTPUTILS_H

#include "httplib.h"
#include "json.hpp"
#include <optional>
#include <stdexcept>
#include <string>

class BadRequest : public std::runtime_error
{
public:
    explicit BadRequest(const std::string& message) : std::runtime_error(message) {}
};

void sendJson(httplib::Response& res, int status, const nlohmann::json& body);
void sendError(httplib::Response& res, int status, const std::string& message);

std::string textParam(const httplib::Request& req, const char* name, size_t maxLength = 200);
int intParam(const httplib::Request& req, const char* name, int fallback, int min, int max);
std::optional<double> numberParam(const httplib::Request& req, const char* name, double min, double max);

nlohmann::json jsonBody(const httplib::Request& req);
std::string jsonText(const nlohmann::json& body, const char* field, size_t maxLength);

std::string trim(const std::string& text);

#endif
