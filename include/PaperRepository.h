#ifndef PAPERREPOSITORY_H
#define PAPERREPOSITORY_H

#include "Database.h"
#include "Paper.h"
#include "SearchEngine.h"
#include "json.hpp"
#include <optional>
#include <string>
#include <vector>

// Everything the user can filter and sort by. Empty / 0 / nullopt = no filter.
struct PaperFilter
{
    std::string author;              // part of an author name, case-insensitive
    std::string journal;             // part of a journal name, case-insensitive
    std::string category;            // exact research area
    std::string keyword;             // exact keyword, case-insensitive
    std::string type;                // publication type: article, review, ...
    int year = 0;                    // one exact year
    int yearFrom = 0;
    int yearTo = 0;
    std::optional<double> minImpact;
    std::optional<double> maxImpact;
    std::string sort = "relevance";  // relevance, newest, oldest, impact_desc, impact_asc, citations, author
    int page = 1;
    int pageSize = 10;

    bool hasAnyFilter() const;
    nlohmann::json toJson() const;
};

bool isValidSort(const std::string& sort);

// All SQL about papers. The C++ engine decides WHICH papers match a text query (and how
// well); PostgreSQL then applies the filters, sorting and pagination using its indexes.
class PaperRepository
{
public:
    explicit PaperRepository(Database& db);

    // Every paper with its authors and keywords, to build the SearchEngine.
    std::vector<Paper> loadAll();

    // Changes whenever a paper is added, removed or updated.
    std::string dataVersion();

    // hits == nullptr: no text query, filter all papers. userId 0 = not logged in.
    nlohmann::json search(const std::vector<SearchHit>* hits, const PaperFilter& filter, int userId);

    std::optional<nlohmann::json> details(int paperId, int userId);

    // Short paper records (for cards), in the order of ids.
    nlohmann::json summaries(const std::vector<int>& ids, int userId);

    nlohmann::json categories();
    nlohmann::json filterOptions();
    nlohmann::json featured(const std::string& category, int limit, int userId);

private:
    Database& db_;
};

// Turns one row of a query that selected SUMMARY_COLUMNS into JSON.
nlohmann::json paperSummaryJson(const Result& result, int row);

#endif
