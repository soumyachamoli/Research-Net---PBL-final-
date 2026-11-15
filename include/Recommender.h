#ifndef RECOMMENDER_H
#define RECOMMENDER_H

#include "SearchEngine.h"
#include <string>
#include <vector>

struct RecommenderWeights
{
    double content = 0.50;
    double keywords = 0.20;
    double authors = 0.15;
    double recency = 0.10;
    double impact = 0.05;

    bool parse(const std::string& text);
};

struct Recommendation
{
    int paperId = 0;
    double score = 0;
    double similarity = 0;
    double content = 0;
    double keywords = 0;
    double authors = 0;
    double recency = 0;
    double impact = -1;
    std::vector<std::string> reasons;
};

struct UserProfile
{
    std::vector<int> savedPaperIds;
    std::vector<int> viewedPaperIds;
    std::vector<std::string> searches;
    std::vector<std::string> interests;
};

class Recommender
{
public:
    Recommender(const SearchEngine& engine, RecommenderWeights weights);

    std::vector<Recommendation> similarTo(int paperId, size_t count) const;

    std::vector<Recommendation> forUser(const UserProfile& profile, size_t count) const;

private:
    const SearchEngine& engine_;
    RecommenderWeights weights_;

    double recencyScore(const Paper& paper) const;
    double impactScore(const Paper& paper) const;
    double combine(Recommendation& rec) const;
    std::vector<Recommendation> rank(std::vector<Recommendation>& candidates, size_t count) const;
    std::vector<Recommendation> topK(std::vector<Recommendation>& candidates, size_t count) const;
};

#endif
