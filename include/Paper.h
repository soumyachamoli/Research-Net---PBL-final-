#ifndef PAPER_H
#define PAPER_H

#include <optional>
#include <string>
#include <vector>

// The part of a paper the search engine keeps in memory. The full record (DOI, URL,
// complete abstract for display, ...) is always read from PostgreSQL when it is shown.
struct Paper
{
    int id = 0;                          // papers.id in PostgreSQL
    std::string title;
    std::string abstractText;
    int year = 0;                        // 0 = unknown
    std::string category;                // research area, e.g. "Artificial Intelligence"
    std::string topic;
    std::string journal;
    std::optional<double> impactFactor;  // empty = not known (never treated as zero)
    int citedBy = 0;
    std::vector<int> authorIds;
    std::vector<std::string> authors;
    std::vector<std::string> keywords;
};

#endif
