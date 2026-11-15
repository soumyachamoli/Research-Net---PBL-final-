#ifndef SEARCHENGINE_H
#define SEARCHENGINE_H

#include "Database.h"
#include "History.h"
#include "InvertedIndex.h"
#include "LinkedList.h"
#include "Paper.h"
#include "Trie.h"

// Sort options for search results.
const int SORT_DEFAULT = 0;        // by paper ID
const int SORT_IMPACT_HIGH = 1;    // highest impact factor first
const int SORT_IMPACT_LOW = 2;     // lowest impact factor first
const int SORT_NEWEST = 3;         // newest year first

// What the user chose in the filter panel. 0 or "" means "no filter".
struct SearchFilter
{
    int yearFrom;
    int yearTo;
    char author[100];
    char quartile[QUARTILE_SIZE];
    float minImpact;
    int sortBy;
};

// One recommended paper and why it was recommended.
struct Recommendation
{
    int paperId;
    int score;
    int commonKeywords;
    int commonAuthors;
};

// Holds the papers loaded from PostgreSQL and the data structures built from them.
class SearchEngine
{
private:
    Paper* papers;           // dynamic array of papers
    int paperCount;
    Trie trie;
    InvertedIndex index;
    History history;

    void addPaperToStructures(const Paper& paper);
    int findIndex(int id) const;
    bool passesFilter(const Paper& paper, const SearchFilter& filter) const;
    bool comesBefore(const Paper& a, const Paper& b, int sortBy) const;

public:
    SearchEngine();
    ~SearchEngine();

    bool load(Database& db);
    int getPaperCount() const;
    int getWordCount() const;
    const Paper* findPaper(int id) const;

    void keywordSearch(const char* query, LinkedList& results);
    int searchPapers(const char* query, const SearchFilter& filter, int resultIds[], int maxResults);
    int autocomplete(const char* prefix, char results[][PHRASE_SIZE], int maxResults);
    int recommend(int paperId, Recommendation results[], int maxResults);

    History& getHistory();
};

void clearFilter(SearchFilter& filter);

#endif
