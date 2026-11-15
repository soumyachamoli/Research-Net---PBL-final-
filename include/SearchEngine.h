#ifndef SEARCHENGINE_H
#define SEARCHENGINE_H

#include "InvertedIndex.h"
#include "Paper.h"
#include "Trie.h"
#include <string>
#include <unordered_map>
#include <vector>

struct SearchHit
{
    int paperId;
    double score;
};

class SearchEngine
{
public:
    explicit SearchEngine(std::vector<Paper> papers);

    std::vector<SearchHit> search(const std::string& query) const;

    std::vector<Suggestion> autocomplete(const std::string& prefix, size_t maxResults) const;

    const Paper* findPaper(int id) const;
    int findIndex(int id) const;
    const std::vector<Paper>& papers() const;
    const InvertedIndex& index() const;
    const std::vector<int>& papersByAuthor(int authorId) const;
    const std::vector<int>& papersWithKeyword(const std::string& keyword) const;

    int minYear() const;
    int maxYear() const;
    double maxImpactFactor() const;
    size_t trieNodes() const;

private:
    std::vector<Paper> papers_;
    InvertedIndex index_;
    Trie trie_;
    std::unordered_map<int, int> positionById_;
    std::unordered_map<int, std::vector<int>> papersByAuthor_;
    std::unordered_map<std::string, std::vector<int>> papersByKeyword_;
    int minYear_ = 0;
    int maxYear_ = 0;
    double maxImpact_ = 0;

    void addToIndex(int position, const Paper& paper);
    void addToTrie(const Paper& paper);
};

#endif
