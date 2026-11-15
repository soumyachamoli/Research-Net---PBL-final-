#include "Trie.h"
#include "TextUtils.h"
#include <algorithm>
using namespace std;

static const size_t MAX_KEY_LENGTH = 80;

Trie::Trie()
{
    nodes_.emplace_back();
}

int Trie::findChild(int node, char c) const
{
    for (const auto& child : nodes_[node].children)
    {
        if (child.first == c)
        {
            return child.second;
        }
    }
    return -1;
}

int Trie::addChild(int node, char c)
{
    int child = findChild(node, c);
    if (child != -1)
    {
        return child;
    }
    nodes_.emplace_back();
    child = (int)nodes_.size() - 1;
    nodes_[node].children.push_back({c, child});
    return child;
}

void Trie::insert(const string& text, const string& kind, double weight, int paperId)
{
    string key = normalizeKey(text);
    if (key.empty())
    {
        return;
    }
    if (key.size() > MAX_KEY_LENGTH)
    {
        key.resize(MAX_KEY_LENGTH);
    }

    auto found = entryByKey_.find(key);
    if (found != entryByKey_.end())
    {
        Suggestion& existing = entries_[found->second];
        existing.weight += weight;
        return;
    }

    int current = 0;
    for (char c : key)
    {
        current = addChild(current, c);
    }
    entries_.push_back({text, kind, weight, paperId});
    nodes_[current].entry = (int)entries_.size() - 1;
    entryByKey_[key] = nodes_[current].entry;
}

bool Trie::better(int a, int b) const
{
    if (entries_[a].weight != entries_[b].weight)
    {
        return entries_[a].weight > entries_[b].weight;
    }
    return entries_[a].text < entries_[b].text;
}

void Trie::collectBest(int node)
{
    vector<int> candidates;
    if (nodes_[node].entry != -1)
    {
        candidates.push_back(nodes_[node].entry);
    }
    for (const auto& child : nodes_[node].children)
    {
        collectBest(child.second);
        const vector<int>& childBest = nodes_[child.second].best;
        candidates.insert(candidates.end(), childBest.begin(), childBest.end());
    }
    sort(candidates.begin(), candidates.end(), [this](int a, int b) { return better(a, b); });
    if (candidates.size() > TOP_K)
    {
        candidates.resize(TOP_K);
    }
    nodes_[node].best = candidates;
}

void Trie::finalize()
{
    collectBest(0);
}

int Trie::findNode(const string& key) const
{
    int current = 0;
    for (char c : key)
    {
        current = findChild(current, c);
        if (current == -1)
        {
            return -1;
        }
    }
    return current;
}

bool Trie::contains(const string& text) const
{
    int node = findNode(normalizeKey(text));
    return node != -1 && nodes_[node].entry != -1;
}

vector<Suggestion> Trie::autocomplete(const string& prefix, size_t maxResults) const
{
    vector<Suggestion> results;
    string key = normalizeKey(prefix);
    if (!prefix.empty() && prefix.back() == ' ' && !key.empty())
    {
        key += ' ';
    }
    if (key.empty())
    {
        return results;
    }
    int node = findNode(key);
    if (node == -1)
    {
        return results;
    }
    for (int entry : nodes_[node].best)
    {
        if (results.size() >= maxResults)
        {
            break;
        }
        results.push_back(entries_[entry]);
    }
    return results;
}

size_t Trie::nodeCount() const
{
    return nodes_.size();
}

size_t Trie::size() const
{
    return entries_.size();
}
