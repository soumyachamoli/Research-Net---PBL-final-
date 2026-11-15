#ifndef INVERTEDINDEX_H
#define INVERTEDINDEX_H

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// One entry of a posting list: this document contains the term, with this TF-IDF weight.
struct Posting
{
    int doc;
    float weight;
};

// A document as a sparse TF-IDF vector: (term id, weight) pairs, length 1 (normalized).
using SparseVector = std::vector<std::pair<int, float>>;

// Inverted Index: term -> list of documents that contain it.
//
//   "network" -> [(doc 2, 0.11), (doc 10, 0.07), (doc 28, 0.15) ...]
//
// Terms are stored in an unordered_map (hash table) from the word to a term id, and each
// term id has a posting list (vector). Weights are TF-IDF:
//   tf-idf(t, d) = (1 + ln tf) * idf(t)       idf(t) = ln((N + 1) / (df(t) + 1)) + 1
// where tf = how often t occurs in d (field weighted), df = how many documents contain t,
// N = number of documents. Each document vector is then divided by its length, so the
// dot product of two vectors is their cosine similarity.
class InvertedIndex
{
public:
    // doc must be 0, 1, 2 ... in order. termCounts: term -> (weighted) count in this document.
    void addDocument(int doc, const std::unordered_map<std::string, float>& termCounts);

    // Computes idf values and normalized TF-IDF weights. Call once after the last addDocument().
    void finalize();

    // Ranked keyword search. requireAll = true: AND (every term must occur);
    // false: OR (any term). Returns (doc, cosine score) pairs, best first.
    std::vector<std::pair<int, double>> search(const std::vector<std::string>& terms, bool requireAll) const;

    // Turns query words into a normalized TF-IDF vector (unknown words are ignored).
    SparseVector queryVector(const std::vector<std::string>& terms) const;

    int termId(const std::string& term) const;            // -1 if the term is not indexed
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
    std::vector<std::vector<Posting>> postings_;           // indexed by term id
    std::vector<double> idf_;                              // indexed by term id
    std::vector<SparseVector> documents_;                  // indexed by doc
};

double dot(const SparseVector& a, const SparseVector& b);  // both sorted by term id

#endif
