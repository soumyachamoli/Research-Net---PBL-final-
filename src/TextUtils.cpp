#include "TextUtils.h"
#include <cstring>

void copyText(char* dest, const char* src, int destSize)
{
    int i = 0;
    while (src[i] != '\0' && i < destSize - 1)
    {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static char toLower(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return c - 'A' + 'a';
    }
    return c;
}

static bool isLetter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int splitTokens(const char* text, char words[][WORD_SIZE], int maxWords)
{
    int count = 0;
    int length = 0;       // length of the word being built

    for (int i = 0; ; i++)
    {
        char c = text[i];
        if (isLetter(c))
        {
            if (length < WORD_SIZE - 1 && count < maxWords)
            {
                words[count][length] = toLower(c);
                length++;
            }
        }
        else
        {
            // Any non-letter ends the current word.
            if (length > 0 && count < maxWords)
            {
                words[count][length] = '\0';
                count++;
            }
            length = 0;
        }
        if (c == '\0')
        {
            break;
        }
    }
    return count;
}

bool isStopWord(const char* word)
{
    const char* stopWords[] = {"a", "an", "and", "or", "the", "of", "in", "on", "for", "to",
                               "with", "by", "is", "are", "as", "at", "from", "into", "its",
                               "it", "that", "this", "be", "we", "our", "via", "using"};
    int total = sizeof(stopWords) / sizeof(stopWords[0]);

    if (strlen(word) < 2)
    {
        return true;     // single letters are not useful search words
    }
    for (int i = 0; i < total; i++)
    {
        if (strcmp(word, stopWords[i]) == 0)
        {
            return true;
        }
    }
    return false;
}

int splitWords(const char* text, char words[][WORD_SIZE], int maxWords)
{
    char tokens[MAX_WORDS][WORD_SIZE];
    int tokenCount = splitTokens(text, tokens, MAX_WORDS);
    int count = 0;

    for (int i = 0; i < tokenCount && count < maxWords; i++)
    {
        if (isStopWord(tokens[i]))
        {
            continue;
        }
        bool alreadyAdded = false;
        for (int j = 0; j < count; j++)
        {
            if (strcmp(words[j], tokens[i]) == 0)
            {
                alreadyAdded = true;
                break;
            }
        }
        if (!alreadyAdded)
        {
            copyText(words[count], tokens[i], WORD_SIZE);
            count++;
        }
    }
    return count;
}

int splitPhrases(const char* text, char phrases[][PHRASE_SIZE], int maxPhrases)
{
    int count = 0;
    int length = 0;

    for (int i = 0; ; i++)
    {
        char c = text[i];
        if (c == ',' || c == '\0')
        {
            // Remove spaces at the end of the phrase.
            while (length > 0 && phrases[count][length - 1] == ' ')
            {
                length--;
            }
            if (length > 0 && count < maxPhrases)
            {
                phrases[count][length] = '\0';
                count++;
            }
            length = 0;
            if (c == '\0')
            {
                break;
            }
        }
        else if (count < maxPhrases && length < PHRASE_SIZE - 1)
        {
            if (c == ' ' && length == 0)
            {
                continue;    // skip spaces at the start of the phrase
            }
            phrases[count][length] = toLower(c);
            length++;
        }
    }
    return count;
}

bool containsIgnoreCase(const char* text, const char* part)
{
    if (part[0] == '\0')
    {
        return true;
    }
    for (int start = 0; text[start] != '\0'; start++)
    {
        int i = 0;
        while (part[i] != '\0' && text[start + i] != '\0' &&
               toLower(text[start + i]) == toLower(part[i]))
        {
            i++;
        }
        if (part[i] == '\0')
        {
            return true;      // every character of part matched
        }
    }
    return false;
}
