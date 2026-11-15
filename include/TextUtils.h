#ifndef TEXTUTILS_H
#define TEXTUTILS_H

// Text processing used by the Trie, the Inverted Index and the search.

const int MAX_WORDS = 400;     // most words we take from one text
const int WORD_SIZE = 50;      // longest single word
const int PHRASE_SIZE = 100;   // longest keyword phrase, e.g. "machine learning"

// Copies src into dest without writing past destSize (always ends with '\0').
void copyText(char* dest, const char* src, int destSize);

// Splits text into lowercase words made of letters a-z only.
// "Deep Learning for NLP!" -> "deep", "learning", "for", "nlp"
int splitTokens(const char* text, char words[][WORD_SIZE], int maxWords);

// Same as splitTokens, but also removes stop words ("the", "of", "and", ...)
// and removes repeated words.
int splitWords(const char* text, char words[][WORD_SIZE], int maxWords);

// Splits "machine learning, Healthcare" into "machine learning" and "healthcare".
int splitPhrases(const char* text, char phrases[][PHRASE_SIZE], int maxPhrases);

bool isStopWord(const char* word);

// true if `part` appears anywhere in `text`, ignoring upper/lower case.
bool containsIgnoreCase(const char* text, const char* part);

#endif
