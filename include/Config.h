#ifndef CONFIG_H
#define CONFIG_H

#include "Recommender.h"
#include <string>

struct Config
{
    std::string databaseUrl;
    std::string host = "127.0.0.1";
    int port = 8080;
    int poolSize = 4;
    bool cookieSecure = false;
    bool trustProxy = false;
    int reloadSeconds = 60;
    int historyDays = 180;
    std::string webRoot = "web";
    RecommenderWeights weights;
};

Config loadConfig();

#endif
