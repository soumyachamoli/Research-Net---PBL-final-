#ifndef CONFIG_H
#define CONFIG_H

#include "Recommender.h"
#include <string>

// Settings, read from environment variables (production) or config.txt (local use).
//
//   DATABASE_URL    postgresql://user:password@host:5432/researchnet   (or config.txt, one line)
//   HOST, PORT      where the web server listens (default 127.0.0.1:8080)
//   DB_POOL_SIZE    number of PostgreSQL connections (default 4)
//   COOKIE_SECURE   1 = send the session cookie only over HTTPS (set this in production)
//   TRUST_PROXY     1 = take the client IP from X-Forwarded-For (behind a hosting proxy)
//   RN_WEIGHTS      recommender weights "content,keywords,authors,recency,impact"
//   RELOAD_SECONDS  how often to check PostgreSQL for changed papers (default 60)
//   WEB_ROOT        folder with index.html, app.js, style.css (default "web")
struct Config
{
    std::string databaseUrl;
    std::string host = "127.0.0.1";
    int port = 8080;
    int poolSize = 4;
    bool cookieSecure = false;
    bool trustProxy = false;
    int reloadSeconds = 60;
    std::string webRoot = "web";
    RecommenderWeights weights;
};

// Throws std::runtime_error if no database connection string can be found.
Config loadConfig();

#endif
