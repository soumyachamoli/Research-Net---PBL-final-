#include "Config.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace std;

static string env(const char* name, const string& fallback = "")
{
    const char* value = getenv(name);
    return value != nullptr && value[0] != '\0' ? string(value) : fallback;
}

static int envInt(const char* name, int fallback)
{
    string value = env(name);
    try
    {
        return value.empty() ? fallback : stoi(value);
    }
    catch (...)
    {
        cerr << "Ignoring invalid " << name << "=" << value << endl;
        return fallback;
    }
}

Config loadConfig()
{
    Config config;
    config.databaseUrl = env("DATABASE_URL");
    if (config.databaseUrl.empty())
    {
        // Local use: config.txt holds one libpq connection line. It is in .gitignore,
        // so the password never goes to GitHub.
        ifstream file("config.txt");
        getline(file, config.databaseUrl);
    }
    if (config.databaseUrl.empty())
    {
        throw runtime_error("No database configured. Set DATABASE_URL or copy config.example.txt to config.txt.");
    }

    config.host = env("HOST", config.host);
    config.port = envInt("PORT", config.port);
    config.poolSize = max(1, envInt("DB_POOL_SIZE", config.poolSize));
    config.cookieSecure = env("COOKIE_SECURE") == "1";
    config.trustProxy = env("TRUST_PROXY") == "1";
    config.reloadSeconds = envInt("RELOAD_SECONDS", config.reloadSeconds);
    config.webRoot = env("WEB_ROOT", config.webRoot);
    string weights = env("RN_WEIGHTS");
    if (!weights.empty() && !config.weights.parse(weights))
    {
        cerr << "Ignoring invalid RN_WEIGHTS=" << weights << " (need 5 non-negative numbers)" << endl;
    }
    return config;
}
