#ifndef TRIE_H
#define TRIE_H

#include "TextUtils.h"

const int ALPHABET_SIZE = 27;   // 26 letters + 1 space (for phrases like "machine learning")

struct TrieNode
{
    TrieNode* children[ALPHABET_SIZE];
    bool isEndOfWord;
};

class Trie
{
private:
    TrieNode* root;

    TrieNode* createNode();
    int charToIndex(char c);
    char indexToChar(int index);
    void deleteNode(TrieNode* node);
    void collectWords(TrieNode* node, char* buffer, int depth,
                      char results[][PHRASE_SIZE], int& count, int maxResults);

public:
    Trie();
    ~Trie();

    void insert(const char* word);
    bool search(const char* word);
    int autocomplete(const char* prefix, char results[][PHRASE_SIZE], int maxResults);
};

#endif
