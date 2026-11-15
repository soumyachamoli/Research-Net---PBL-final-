#ifndef RECOMMENDER_H
#define RECOMMENDER_H

#include "SearchEngine.h"
#include <string>
#include <vector>

// Weights of the combined recommendation score. Every component is first normalized to
// 0..1, then:   score = sum(weight_i * component_i) / sum(weight_i)
// A component that is unknown for a paper (impact factor missing) is left out of BOTH
// sums, so missing data neither helps nor hurts. Content similarity has the largest
// weight on purpose: impact factor must never outweigh relevance.
struct RecommenderWeights
{
    double content = 0.50;    // TF-IDF cosine similarity of title + abstract + keywords
    double keywords = 0.20;   // shared author keywords
    double authors = 0.15;    // shared authors
    double recency = 0.10;    // newer papers score higher
    double impact = 0.05;     // journal impact factor, only where known

    // Reads "content,keywords,authors,recency,impact" (e.g. "0.5,0.2,0.15,0.1,0.05").
    // Returns false (and keeps the defaults) if the text is not 5 non-negative numbers.
    bool parse(const std::string& text);
};

struct Recommendation
{
    int paperId = 0;
    double score = 0;          // combined score, 0..1
    double similarity = 0;    // raw TF-IDF cosine similarity (before normalization)
    double content = 0;       // components below are normalized to 0..1
    double keywords = 0;
    double authors = 0;
    double recency = 0;
    double impact = -1;        // -1 = impact factor unknown
    std::vector<std::string> reasons;    // short explanations shown to the user
};

// What we know about a user's interests, all read from PostgreSQL.
struct UserProfile
{
    std::vector<int> savedPaperIds;      // bookmarks, strongest signal
    std::vector<int> viewedPaperIds;     // recently opened papers
    std::vector<std::string> searches;   // recent search queries
    std::vector<std::string> interests;  // interests chosen on the profile page
};

class Recommender
{
public:
    Recommender(const SearchEngine& engine, RecommenderWeights weights);

    // Papers similar to one paper (never the paper itself).
    std::vector<Recommendation> similarTo(int paperId, size_t count) const;

    // Personal recommendations. Papers the user already saved or opened are skipped.
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
