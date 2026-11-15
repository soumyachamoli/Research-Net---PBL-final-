#include "TextUtils.h"
#include <unordered_set>
using namespace std;

static bool isWordChar(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c >= 128;
}

static char toLowerChar(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return c - 'A' + 'a';
    }
    return c;
}

string toLowerCopy(const string& text)
{
    string result = text;
    for (char& c : result)
    {
        c = toLowerChar(c);
    }
    return result;
}

vector<string> tokenize(const string& text)
{
    vector<string> words;
    string current;
    for (char c : text)
    {
        if (isWordChar((unsigned char)c))
        {
            current += toLowerChar(c);
        }
        else if (!current.empty())
        {
            words.push_back(current);       // any other character ends the word
            current.clear();
        }
    }
    if (!current.empty())
    {
        words.push_back(current);
    }
    return words;
}

bool isStopWord(const string& word)
{
    static const unordered_set<string> stopWords = {
        "a", "an", "and", "or", "the", "of", "in", "on", "for", "to", "with", "by", "is", "are",
        "as", "at", "from", "into", "its", "it", "that", "this", "these", "those", "be", "been",
        "was", "were", "we", "our", "us", "via", "using", "based", "which", "can", "also", "has",
        "have", "had", "not", "but", "such", "than", "their", "they", "there", "then", "both",
        "between", "over", "under", "more", "most", "other", "some", "each", "all", "any", "one",
        "two", "new", "use", "used", "paper", "propose", "proposed", "approach", "method", "show",
        "results", "result", "while", "however", "how", "what", "when", "where", "who", "will",
        "may", "do", "does", "so", "if", "no", "only", "about", "through", "within", "without"};

    if (word.size() < 2)
    {
        return true;                        // single characters are not useful search words
    }
    return stopWords.count(word) > 0;
}

static bool endsWith(const string& word, const string& suffix)
{
    return word.size() >= suffix.size() &&
           word.compare(word.size() - suffix.size(), suffix.size(), suffix) == 0;
}

string stem(const string& word)
{
    if (word.size() <= 3)
    {
        return word;
    }
    // Plurals.
    if (endsWith(word, "ies") && word.size() > 4)
    {
        return word.substr(0, word.size() - 3) + "y";          // studies -> study
    }
    if (endsWith(word, "sses"))
    {
        return word.substr(0, word.size() - 2);                // classes -> class
    }
    if (endsWith(word, "s") && !endsWith(word, "ss") && !endsWith(word, "us") && !endsWith(word, "is"))
    {
        return word.substr(0, word.size() - 1);                // networks -> network
    }
    // -ing / -ed, only when a reasonable stem (4+ letters) is left.
    if (endsWith(word, "ing") && word.size() >= 7)
    {
        return word.substr(0, word.size() - 3);                // learning -> learn
    }
    if (endsWith(word, "ed") && word.size() >= 6)
    {
        return word.substr(0, word.size() - 2);                // trained -> train
    }
    return word;
}

vector<string> analyze(const string& text)
{
    vector<string> terms;
    for (const string& word : tokenize(text))
    {
        if (!isStopWord(word))
        {
            terms.push_back(stem(word));
        }
    }
    return terms;
}

string normalizeKey(const string& text)
{
    string key;
    for (const string& word : tokenize(text))
    {
        if (!key.empty())
        {
            key += ' ';
        }
        key += word;
    }
    return key;
}

bool containsIgnoreCase(const string& text, const string& part)
{
    return toLowerCopy(text).find(toLowerCopy(part)) != string::npos;
}
