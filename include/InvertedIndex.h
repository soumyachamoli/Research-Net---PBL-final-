#ifndef INVERTEDINDEX_H
#define INVERTEDINDEX_H

#include "LinkedList.h"
#include "TextUtils.h"

const int TABLE_SIZE = 101;    // number of buckets in the hash table (a prime number)

// One word in the index together with the list of papers that contain it.
struct WordNode
{
    char word[WORD_SIZE];
    LinkedList paperIds;
    WordNode* next;            // next word in the same bucket
};

// Inverted index: word -> linked list of paper IDs.
// Stored as a hash table: an array of buckets, each bucket is a linked list of WordNodes.
class InvertedIndex
{
private:
    WordNode* table[TABLE_SIZE];
    int wordCount;

    int hashFunction(const char* word) const;

public:
    InvertedIndex();
    ~InvertedIndex();

    void insert(const char* word, int paperId);
    LinkedList* search(const char* word);         // nullptr if the word is not indexed
    void display() const;
    int getWordCount() const;
};

#endif
