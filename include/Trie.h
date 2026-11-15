#ifndef TRIE_H
#define TRIE_H

#include <string>
#include <unordered_map>
#include <vector>

struct Suggestion
{
    std::string text;
    std::string kind;
    double weight;
    int paperId;
};

class Trie
{
public:
    static const size_t TOP_K = 10;

    Trie();

    void insert(const std::string& text, const std::string& kind, double weight, int paperId = 0);

    void finalize();

    bool contains(const std::string& text) const;
    std::vector<Suggestion> autocomplete(const std::string& prefix, size_t maxResults) const;

    size_t nodeCount() const;
    size_t size() const;

private:
    struct Node
    {
        std::vector<std::pair<char, int>> children;
        int entry = -1;
        std::vector<int> best;
    };

    std::vector<Node> nodes_;
    std::vector<Suggestion> entries_;
    std::unordered_map<std::string, int> entryByKey_;

    int findChild(int node, char c) const;
    int addChild(int node, char c);
    int findNode(const std::string& key) const;
    void collectBest(int node);
    bool better(int a, int b) const;
};

#endif
