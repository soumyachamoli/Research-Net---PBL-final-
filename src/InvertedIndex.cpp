#include "InvertedIndex.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
using namespace std;

void InvertedIndex::addDocument(int doc, const unordered_map<string, float>& termCounts)
{
    if (doc != (int)documents_.size())
    {
        throw logic_error("documents must be added in order 0, 1, 2, ...");
    }
    SparseVector vector;
    for (const auto& item : termCounts)
    {
        // New word: give it the next term id and an empty posting list.
        auto found = termIds_.find(item.first);
        int id;
        if (found == termIds_.end())
        {
            id = (int)terms_.size();
            termIds_[item.first] = id;
            terms_.push_back(item.first);
            postings_.emplace_back();
        }
        else
        {
            id = found->second;
        }
        postings_[id].push_back({doc, item.second});   // raw count for now, weight in finalize()
        vector.push_back({id, item.second});
    }
    documents_.push_back(vector);
}

void InvertedIndex::finalize()
{
    double n = (double)documents_.size();
    idf_.assign(terms_.size(), 0.0);
    for (size_t id = 0; id < terms_.size(); id++)
    {
        double df = (double)postings_[id].size();
        idf_[id] = log((n + 1.0) / (df + 1.0)) + 1.0;   // rare words count more
    }

    // tf-idf weights, then divide each document vector by its length.
    for (SparseVector& vector : documents_)
    {
        double length = 0;
        for (auto& entry : vector)
        {
            double weight = (1.0 + log(entry.second)) * idf_[entry.first];
            entry.second = (float)weight;
            length += weight * weight;
        }
        length = sqrt(length);
        for (auto& entry : vector)
        {
            entry.second = (float)(entry.second / length);
        }
        sort(vector.begin(), vector.end());            // sorted by term id: needed by dot()
    }

    // Rebuild the posting lists with the final weights (still in document order).
    for (auto& list : postings_)
    {
        list.clear();
    }
    for (size_t doc = 0; doc < documents_.size(); doc++)
    {
        for (const auto& entry : documents_[doc])
        {
            postings_[entry.first].push_back({(int)doc, entry.second});
        }
    }
}

SparseVector InvertedIndex::queryVector(const vector<string>& terms) const
{
    unordered_map<int, double> counts;
    for (const string& term : terms)
    {
        int id = termId(term);
        if (id != -1)
        {
            counts[id] += 1.0;
        }
    }
    SparseVector vector;
    double length = 0;
    for (const auto& item : counts)
    {
        double weight = (1.0 + log(item.second)) * idf_[item.first];
        vector.push_back({item.first, (float)weight});
        length += weight * weight;
    }
    length = sqrt(length);
    for (auto& entry : vector)
    {
        entry.second = (float)(entry.second / length);
    }
    sort(vector.begin(), vector.end());
    return vector;
}

vector<pair<int, double>> InvertedIndex::search(const vector<string>& terms, bool requireAll) const
{
    vector<pair<int, double>> results;
    unordered_set<string> distinct(terms.begin(), terms.end());
    if (distinct.empty())
    {
        return results;
    }
    if (requireAll)
    {
        for (const string& term : distinct)
        {
            if (termId(term) == -1)
            {
                return results;                        // AND with an unknown word: nothing matches
            }
        }
    }

    // Walk the posting list of every query term and add up query weight x document weight
    // per document. Only documents that contain at least one term are ever touched.
    SparseVector query = queryVector(terms);
    unordered_map<int, double> scores;
    unordered_map<int, int> matched;                   // how many query terms each doc contains
    for (const auto& entry : query)
    {
        for (const Posting& posting : postings_[entry.first])
        {
            scores[posting.doc] += entry.second * posting.weight;
            matched[posting.doc]++;
        }
    }

    for (const auto& item : scores)
    {
        if (!requireAll || matched[item.first] == (int)query.size())
        {
            results.push_back(item);
        }
    }
    sort(results.begin(), results.end(), [](const pair<int, double>& a, const pair<int, double>& b) {
        if (a.second != b.second)
        {
            return a.second > b.second;
        }
        return a.first < b.first;
    });
    return results;
}

int InvertedIndex::termId(const string& term) const
{
    auto found = termIds_.find(term);
    return found == termIds_.end() ? -1 : found->second;
}

const string& InvertedIndex::termText(int id) const
{
    return terms_.at(id);
}

double InvertedIndex::idf(int id) const
{
    return idf_.at(id);
}

int InvertedIndex::documentFrequency(int id) const
{
    return (int)postings_.at(id).size();
}

const vector<Posting>& InvertedIndex::postings(int id) const
{
    return postings_.at(id);
}

const SparseVector& InvertedIndex::documentVector(int doc) const
{
    return documents_.at(doc);
}

size_t InvertedIndex::termCount() const
{
    return terms_.size();
}

size_t InvertedIndex::documentCount() const
{
    return documents_.size();
}

// Merge-style dot product of two vectors sorted by term id.
double dot(const SparseVector& a, const SparseVector& b)
{
    double sum = 0;
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size())
    {
        if (a[i].first == b[j].first)
        {
            sum += (double)a[i].second * b[j].second;
            i++;
            j++;
        }
        else if (a[i].first < b[j].first)
        {
            i++;
        }
        else
        {
            j++;
        }
    }
    return sum;
}
