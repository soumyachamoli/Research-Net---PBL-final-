#ifndef INVERTEDINDEX_H
#define INVERTEDINDEX_H

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct Posting
{
    int doc;
    float weight;
};

using SparseVector = std::vector<std::pair<int, float>>;

class InvertedIndex
{
public:
    void addDocument(int doc, const std::unordered_map<std::string, float>& termCounts);

    void finalize();

    std::vector<std::pair<int, double>> search(const std::vector<std::string>& terms, bool requireAll) const;

    SparseVector queryVector(const std::vector<std::string>& terms) const;

    int termId(const std::string& term) const;
    const std::string& termText(int id) const;
    double idf(int id) const;
    int documentFrequency(int id) const;
    const std::vector<Posting>& postings(int id) const;
    const SparseVector& documentVector(int doc) const;

    size_t termCount() const;
    size_t documentCount() const;

private:
    std::unordered_map<std::string, int> termIds_;
    std::vector<std::string> terms_;
    std::vector<std::vector<Posting>> postings_;
    std::vector<double> idf_;
    std::vector<SparseVector> documents_;
};

double dot(const SparseVector& a, const SparseVector& b);

#endif
