#ifndef TEXTUTILS_H
#define TEXTUTILS_H

#include <string>
#include <vector>


std::string toLowerCopy(const std::string& text);

std::vector<std::string> tokenize(const std::string& text);

bool isStopWord(const std::string& word);

std::string stem(const std::string& word);

std::vector<std::string> analyze(const std::string& text);

std::string normalizeKey(const std::string& text);

bool containsIgnoreCase(const std::string& text, const std::string& part);

#endif
