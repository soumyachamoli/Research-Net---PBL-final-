#ifndef PAPER_H
#define PAPER_H

#include <optional>
#include <string>
#include <vector>

struct Paper
{
    int id = 0;
    std::string title;
    std::string abstractText;
    int year = 0;
    std::string category;
    std::string topic;
    std::string journal;
    std::optional<double> impactFactor;
    int citedBy = 0;
    std::vector<int> authorIds;
    std::vector<std::string> authors;
    std::vector<std::string> keywords;
};

#endif
