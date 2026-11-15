#include "InvertedIndex.h"
#include <cstring>
#include <iostream>
using namespace std;

InvertedIndex::InvertedIndex()
{
    for (int i = 0; i < TABLE_SIZE; i++)
    {
        table[i] = nullptr;
    }
    wordCount = 0;
}

InvertedIndex::~InvertedIndex()
{
    for (int i = 0; i < TABLE_SIZE; i++)
    {
        WordNode* current = table[i];
        while (current != nullptr)
        {
            WordNode* temp = current;
            current = current->next;
            delete temp;          // also deletes its LinkedList of paper IDs (destructor)
        }
    }
}

// Turns a word into a bucket number from 0 to TABLE_SIZE - 1.
// The same word always gives the same number.
int InvertedIndex::hashFunction(const char* word) const
{
    unsigned long hash = 0;
    for (int i = 0; word[i] != '\0'; i++)
    {
        hash = hash * 31 + (unsigned char)word[i];
    }
    return (int)(hash % TABLE_SIZE);
}

void InvertedIndex::insert(const char* word, int paperId)
{
    int bucket = hashFunction(word);

    // Is the word already in this bucket? Then just add the paper ID to its list.
    WordNode* current = table[bucket];
    while (current != nullptr)
    {
        if (strcmp(current->word, word) == 0)
        {
            current->paperIds.insert(paperId);
            return;
        }
        current = current->next;
    }

    // New word: create a node and put it at the front of the bucket.
    WordNode* node = new WordNode;
    copyText(node->word, word, WORD_SIZE);
    node->paperIds.insert(paperId);
    node->next = table[bucket];
    table[bucket] = node;
    wordCount++;
}

LinkedList* InvertedIndex::search(const char* word)
{
    int bucket = hashFunction(word);
    WordNode* current = table[bucket];
    while (current != nullptr)
    {
        if (strcmp(current->word, word) == 0)
        {
            return &current->paperIds;
        }
        current = current->next;
    }
    return nullptr;
}

void InvertedIndex::display() const
{
    for (int i = 0; i < TABLE_SIZE; i++)
    {
        WordNode* current = table[i];
        while (current != nullptr)
        {
            cout << current->word << " -> ";
            current->paperIds.display();
            current = current->next;
        }
    }
}

int InvertedIndex::getWordCount() const
{
    return wordCount;
}
