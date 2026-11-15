#ifndef SEARCHENGINE_H
#define SEARCHENGINE_H

#include "InvertedIndex.h"
#include "Paper.h"
#include "Trie.h"
#include <string>
#include <unordered_map>
#include <vector>

// One search result: a paper id and its relevance score (cosine similarity, 0..1).
struct SearchHit
{
    int paperId;
    double score;
};

// Holds the papers loaded from PostgreSQL and the data structures built from them:
//   - Trie            : autocomplete for titles, keywords, topics, categories, authors
//   - InvertedIndex   : ranked keyword search and the TF-IDF vectors for recommendations
//   - unordered_maps  : paper id -> position, author -> papers, keyword -> papers
// The engine is built once and then only read, so many requests can use it at the same time.
class SearchEngine
{
public:
    explicit SearchEngine(std::vector<Paper> papers);

    // "machine learning"        -> papers containing machine AND learning (default)
    // "vision OR translation"   -> papers containing vision OR translation
    // Results are ranked by TF-IDF cosine similarity to the query.
    std::vector<SearchHit> search(const std::string& query) const;

    std::vector<Suggestion> autocomplete(const std::string& prefix, size_t maxResults) const;

    const Paper* findPaper(int id) const;      // nullptr if unknown
    int findIndex(int id) const;               // position in papers(), or -1
    const std::vector<Paper>& papers() const;
    const InvertedIndex& index() const;
    const std::vector<int>& papersByAuthor(int authorId) const;
    const std::vector<int>& papersWithKeyword(const std::string& keyword) const;   // lower-case keyword

    int minYear() const;
    int maxYear() const;
    double maxImpactFactor() const;
    size_t trieNodes() const;

private:
    std::vector<Paper> papers_;
    InvertedIndex index_;
    Trie trie_;
    std::unordered_map<int, int> positionById_;
    std::unordered_map<int, std::vector<int>> papersByAuthor_;            // author id -> positions
    std::unordered_map<std::string, std::vector<int>> papersByKeyword_;   // keyword -> positions
    int minYear_ = 0;
    int maxYear_ = 0;
    double maxImpact_ = 0;

    void addToIndex(int position, const Paper& paper);
    void addToTrie(const Paper& paper);
};

#endif
