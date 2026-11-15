#ifndef TEXTUTILS_H
#define TEXTUTILS_H

#include <string>
#include <vector>

// Text processing shared by the Trie, the Inverted Index and the recommender.
//
//   "Deep Networks for Image-Classification"
//      tokenize  -> deep, networks, for, image, classification
//      analyze   -> deep, network, image, classif...   (stop words removed, light stemming)

std::string toLowerCopy(const std::string& text);

// Splits text into lower-case words. Letters and digits are kept (so "gpt4" and "5g"
// survive); every other ASCII character ends a word. Bytes >= 128 (UTF-8 letters such
// as the "u" in "Muller" with an umlaut) are kept inside words.
std::vector<std::string> tokenize(const std::string& text);

bool isStopWord(const std::string& word);

// Light English stemmer: "networks" -> "network", "studies" -> "study",
// "learning" -> "learn". Queries and papers go through the same function, so a search
// for "transformer" also finds "transformers".
std::string stem(const std::string& word);

// tokenize + remove stop words + stem: the words that go into the inverted index.
std::vector<std::string> analyze(const std::string& text);

// Key used by the Trie: lower case, letters/digits/spaces only, single spaces.
// "  Machine-Learning " -> "machine learning"
std::string normalizeKey(const std::string& text);

bool containsIgnoreCase(const std::string& text, const std::string& part);

#endif
