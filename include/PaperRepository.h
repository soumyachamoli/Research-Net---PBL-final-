#ifndef PAPERREPOSITORY_H
#define PAPERREPOSITORY_H

#include "Database.h"
#include "Paper.h"
#include "SearchEngine.h"
#include "json.hpp"
#include <optional>
#include <string>
#include <vector>

struct PaperFilter
{
    std::string author;
    std::string journal;
    std::string category;
    std::string keyword;
    std::string type;
    int year = 0;
    int yearFrom = 0;
    int yearTo = 0;
    std::optional<double> minImpact;
    std::optional<double> maxImpact;
    std::string sort = "relevance";
    int page = 1;
    int pageSize = 10;

    bool hasAnyFilter() const;
    nlohmann::json toJson() const;
};

bool isValidSort(const std::string& sort);

class PaperRepository
{
public:
    explicit PaperRepository(Database& db);

    std::vector<Paper> loadAll();

    std::string dataVersion();

    nlohmann::json search(const std::vector<SearchHit>* hits, const PaperFilter& filter, int userId);

    std::optional<nlohmann::json> details(int paperId, int userId);

    nlohmann::json summaries(const std::vector<int>& ids, int userId);

    nlohmann::json categories();
    nlohmann::json filterOptions();
    nlohmann::json featured(const std::string& category, int limit, int userId);

private:
    Database& db_;
};

nlohmann::json paperSummaryJson(const Result& result, int row);

#endif
