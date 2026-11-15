#ifndef TRIE_H
#define TRIE_H

#include <string>
#include <unordered_map>
#include <vector>

// One autocomplete suggestion: what to show, what kind of thing it is, and how important.
struct Suggestion
{
    std::string text;      // shown to the user, e.g. "Attention Is All You Need"
    std::string kind;      // "paper", "keyword", "topic", "author" or "category"
    double weight;         // higher = shown first (citations, number of papers, ...)
    int paperId;           // for kind "paper": the paper to open; otherwise 0
};

// Trie (prefix tree) for search autocomplete.
//
// Every stored phrase is a path from the root: "deep learning" = d -> e -> e -> p -> ' ' -> l ...
// Phrases that start the same way share nodes. After all phrases are inserted, finalize()
// stores in every node the K best suggestions found anywhere below it, so autocomplete()
// only has to walk down the prefix: O(prefix length), no matter how many phrases there are.
class Trie
{
public:
    static const size_t TOP_K = 10;

    Trie();

    // Adds a phrase. The key is normalized (case-insensitive), and adding the same phrase
    // twice does not create a duplicate suggestion: the weights are added up instead.
    void insert(const std::string& text, const std::string& kind, double weight, int paperId = 0);

    // Must be called once after the last insert().
    void finalize();

    bool contains(const std::string& text) const;
    std::vector<Suggestion> autocomplete(const std::string& prefix, size_t maxResults) const;

    size_t nodeCount() const;
    size_t size() const;   // number of distinct phrases

private:
    struct Node
    {
        std::vector<std::pair<char, int>> children;   // (character, index of child node)
        int entry = -1;                               // index into entries_ if a phrase ends here
        std::vector<int> best;                        // top-K entries in this subtree
    };

    std::vector<Node> nodes_;                         // nodes_[0] is the root
    std::vector<Suggestion> entries_;
    std::unordered_map<std::string, int> entryByKey_; // normalized key -> entry (no duplicates)

    int findChild(int node, char c) const;
    int addChild(int node, char c);
    int findNode(const std::string& key) const;
    void collectBest(int node);
    bool better(int a, int b) const;
};

#endif
