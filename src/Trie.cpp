#include "Trie.h"

TrieNode* Trie::createNode()
{
    TrieNode* node = new TrieNode;
    for (int i = 0; i < ALPHABET_SIZE; i++)
    {
        node->children[i] = nullptr;
    }
    node->isEndOfWord = false;
    return node;
}

// 'a'..'z' -> 0..25, space -> 26. Uppercase counts as lowercase. Other characters -> -1 (skipped).
int Trie::charToIndex(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        c = c - 'A' + 'a';
    }
    if (c >= 'a' && c <= 'z')
    {
        return c - 'a';
    }
    if (c == ' ')
    {
        return 26;
    }
    return -1;
}

char Trie::indexToChar(int index)
{
    if (index == 26)
    {
        return ' ';
    }
    return 'a' + index;
}

Trie::Trie()
{
    root = createNode();
}

Trie::~Trie()
{
    deleteNode(root);
}

// Deletes a node and everything below it: children first, then the node itself.
void Trie::deleteNode(TrieNode* node)
{
    if (node == nullptr)
    {
        return;
    }
    for (int i = 0; i < ALPHABET_SIZE; i++)
    {
        deleteNode(node->children[i]);
    }
    delete node;
}

void Trie::insert(const char* word)
{
    TrieNode* current = root;
    bool addedSomething = false;

    for (int i = 0; word[i] != '\0'; i++)
    {
        int index = charToIndex(word[i]);
        if (index == -1)
        {
            continue;
        }
        if (current->children[index] == nullptr)
        {
            current->children[index] = createNode();   // the path does not exist yet: build it
        }
        current = current->children[index];            // go one level down
        addedSomething = true;
    }

    if (addedSomething)
    {
        current->isEndOfWord = true;                   // mark where the word ends
    }
}

bool Trie::search(const char* word)
{
    TrieNode* current = root;

    for (int i = 0; word[i] != '\0'; i++)
    {
        int index = charToIndex(word[i]);
        if (index == -1)
        {
            continue;
        }
        if (current->children[index] == nullptr)
        {
            return false;                              // the path stops: word not stored
        }
        current = current->children[index];
    }
    return current->isEndOfWord;
}

// Visits every node below `node` (depth-first). `buffer` holds the letters on the
// current path; whenever a word ends, the buffer is copied into results.
void Trie::collectWords(TrieNode* node, char* buffer, int depth,
                        char results[][PHRASE_SIZE], int& count, int maxResults)
{
    if (count >= maxResults)
    {
        return;
    }
    if (node->isEndOfWord)
    {
        buffer[depth] = '\0';
        copyText(results[count], buffer, PHRASE_SIZE);
        count++;
    }
    for (int i = 0; i < ALPHABET_SIZE; i++)
    {
        if (node->children[i] != nullptr && depth < PHRASE_SIZE - 1)
        {
            buffer[depth] = indexToChar(i);
            collectWords(node->children[i], buffer, depth + 1, results, count, maxResults);
        }
    }
}

int Trie::autocomplete(const char* prefix, char results[][PHRASE_SIZE], int maxResults)
{
    char buffer[PHRASE_SIZE];
    int depth = 0;
    TrieNode* current = root;

    // Step 1: walk down to the last letter of the prefix.
    for (int i = 0; prefix[i] != '\0' && depth < PHRASE_SIZE - 1; i++)
    {
        int index = charToIndex(prefix[i]);
        if (index == -1)
        {
            continue;
        }
        if (current->children[index] == nullptr)
        {
            return 0;                                  // no word starts with this prefix
        }
        current = current->children[index];
        buffer[depth] = indexToChar(index);
        depth++;
    }
    if (depth == 0)
    {
        return 0;                                      // empty prefix: suggest nothing
    }

    // Step 2: collect every word below that node.
    int count = 0;
    collectWords(current, buffer, depth, results, count, maxResults);
    return count;
}
