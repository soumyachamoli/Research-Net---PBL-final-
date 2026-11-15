#include "SearchEngine.h"
#include <cstring>

void clearFilter(SearchFilter& filter)
{
    filter.yearFrom = 0;
    filter.yearTo = 0;
    filter.author[0] = '\0';
    filter.quartile[0] = '\0';
    filter.minImpact = 0;
    filter.sortBy = SORT_DEFAULT;
}

SearchEngine::SearchEngine()
{
    papers = nullptr;
    paperCount = 0;
}

SearchEngine::~SearchEngine()
{
    delete[] papers;
}

// Puts one paper's words into the Trie and the Inverted Index.
void SearchEngine::addPaperToStructures(const Paper& paper)
{
    char words[MAX_WORDS][WORD_SIZE];
    char phrases[50][PHRASE_SIZE];

    // Inverted index: every word of the title, keywords and abstract -> this paper's id.
    int count = splitWords(paper.title, words, MAX_WORDS);
    for (int i = 0; i < count; i++)
    {
        index.insert(words[i], paper.id);
        trie.insert(words[i]);                 // title words are also autocomplete suggestions
    }
    count = splitWords(paper.keywords, words, MAX_WORDS);
    for (int i = 0; i < count; i++)
    {
        index.insert(words[i], paper.id);
        trie.insert(words[i]);
    }
    count = splitWords(paper.abstractText, words, MAX_WORDS);
    for (int i = 0; i < count; i++)
    {
        index.insert(words[i], paper.id);
    }

    // Trie: whole keyword phrases too, so "mach" can suggest "machine learning".
    count = splitPhrases(paper.keywords, phrases, 50);
    for (int i = 0; i < count; i++)
    {
        trie.insert(phrases[i]);
    }
}

bool SearchEngine::load(Database& db)
{
    paperCount = db.getPapers(papers);
    if (paperCount < 0)
    {
        paperCount = 0;
        return false;
    }
    for (int i = 0; i < paperCount; i++)
    {
        addPaperToStructures(papers[i]);
    }
    return true;
}

int SearchEngine::getPaperCount() const
{
    return paperCount;
}

int SearchEngine::getWordCount() const
{
    return index.getWordCount();
}

// Position of a paper in the papers array, or -1. (Linear search: O(n).)
int SearchEngine::findIndex(int id) const
{
    for (int i = 0; i < paperCount; i++)
    {
        if (papers[i].id == id)
        {
            return i;
        }
    }
    return -1;
}

const Paper* SearchEngine::findPaper(int id) const
{
    int position = findIndex(id);
    if (position == -1)
    {
        return nullptr;
    }
    return &papers[position];
}

// "machine learning"        -> papers with machine AND learning (AND is the default)
// "vision OR translation"   -> papers with vision OR translation
void SearchEngine::keywordSearch(const char* query, LinkedList& results)
{
    char tokens[MAX_WORDS][WORD_SIZE];
    int tokenCount = splitTokens(query, tokens, MAX_WORDS);
    bool firstWord = true;
    bool useOr = false;

    results.clear();
    for (int i = 0; i < tokenCount; i++)
    {
        if (strcmp(tokens[i], "or") == 0)
        {
            useOr = true;
            continue;
        }
        if (strcmp(tokens[i], "and") == 0)
        {
            useOr = false;
            continue;
        }
        if (isStopWord(tokens[i]))
        {
            continue;
        }

        LinkedList* ids = index.search(tokens[i]);     // inverted index lookup
        if (firstWord)
        {
            if (ids != nullptr)
            {
                results.addAll(*ids);
            }
            firstWord = false;
        }
        else if (useOr)
        {
            if (ids != nullptr)
            {
                results.addAll(*ids);                  // OR: union
            }
        }
        else
        {
            if (ids != nullptr)
            {
                results.keepOnlyCommon(*ids);          // AND: intersection
            }
            else
            {
                results.clear();                       // AND with a missing word: nothing matches
            }
        }
        useOr = false;
    }
}

bool SearchEngine::passesFilter(const Paper& paper, const SearchFilter& filter) const
{
    if (filter.yearFrom > 0 && paper.year < filter.yearFrom) return false;
    if (filter.yearTo > 0 && paper.year > filter.yearTo) return false;
    if (filter.author[0] != '\0' && !containsIgnoreCase(paper.authors, filter.author)) return false;
    if (filter.quartile[0] != '\0' && strcmp(paper.quartile, filter.quartile) != 0) return false;
    if (filter.minImpact > 0 && paper.impactFactor < filter.minImpact) return false;
    return true;
}

// true if paper a should be shown before paper b.
bool SearchEngine::comesBefore(const Paper& a, const Paper& b, int sortBy) const
{
    if (sortBy == SORT_IMPACT_HIGH && a.impactFactor != b.impactFactor) return a.impactFactor > b.impactFactor;
    if (sortBy == SORT_IMPACT_LOW && a.impactFactor != b.impactFactor) return a.impactFactor < b.impactFactor;
    if (sortBy == SORT_NEWEST && a.year != b.year) return a.year > b.year;
    return a.id < b.id;                                // otherwise by ID
}

// Search + filter + sort. An empty query means "all papers" (browse with filters only).
// Fills resultIds[] and returns how many results there are.
int SearchEngine::searchPapers(const char* query, const SearchFilter& filter, int resultIds[], int maxResults)
{
    int* positions = new int[paperCount];             // positions in the papers array
    int found = 0;

    // Step 1: candidates from the inverted index (or every paper).
    char tokens[MAX_WORDS][WORD_SIZE];
    if (splitTokens(query, tokens, MAX_WORDS) == 0)
    {
        for (int i = 0; i < paperCount; i++)
        {
            positions[found] = i;
            found++;
        }
    }
    else
    {
        LinkedList matches;
        keywordSearch(query, matches);
        ListNode* current = matches.getHead();
        while (current != nullptr)
        {
            int position = findIndex(current->value);
            if (position != -1)
            {
                positions[found] = position;
                found++;
            }
            current = current->next;
        }
    }

    // Step 2: keep only papers that pass the filters.
    int kept = 0;
    for (int i = 0; i < found; i++)
    {
        if (passesFilter(papers[positions[i]], filter))
        {
            positions[kept] = positions[i];
            kept++;
        }
    }

    // Step 3: insertion sort - take each paper and slide it left to its correct place.
    for (int i = 1; i < kept; i++)
    {
        int current = positions[i];
        int j = i - 1;
        while (j >= 0 && comesBefore(papers[current], papers[positions[j]], filter.sortBy))
        {
            positions[j + 1] = positions[j];
            j--;
        }
        positions[j + 1] = current;
    }

    int count = kept < maxResults ? kept : maxResults;
    for (int i = 0; i < count; i++)
    {
        resultIds[i] = papers[positions[i]].id;
    }
    delete[] positions;
    return count;
}

int SearchEngine::autocomplete(const char* prefix, char results[][PHRASE_SIZE], int maxResults)
{
    return trie.autocomplete(prefix, results, maxResults);
}

static int quartileBonus(const char* quartile)
{
    if (strcmp(quartile, "Q1") == 0) return 4;
    if (strcmp(quartile, "Q2") == 0) return 3;
    if (strcmp(quartile, "Q3") == 0) return 2;
    if (strcmp(quartile, "Q4") == 0) return 1;
    return 0;
}

// Recommendation score for every other paper:
//   score = 3 x (common keyword words) + 5 x (common authors)
//         + impact factor (rounded down) + quartile bonus (Q1=4, Q2=3, Q3=2, Q4=1)
// Only papers that share at least one keyword word or author are recommended.
int SearchEngine::recommend(int paperId, Recommendation results[], int maxResults)
{
    const Paper* base = findPaper(paperId);
    if (base == nullptr)
    {
        return 0;
    }

    char baseWords[MAX_WORDS][WORD_SIZE];
    char otherWords[MAX_WORDS][WORD_SIZE];
    char baseAuthors[30][PHRASE_SIZE];
    char otherAuthors[30][PHRASE_SIZE];
    int baseWordCount = splitWords(base->keywords, baseWords, MAX_WORDS);
    int baseAuthorCount = splitPhrases(base->authors, baseAuthors, 30);

    Recommendation* all = new Recommendation[paperCount];
    int found = 0;

    for (int i = 0; i < paperCount; i++)
    {
        if (papers[i].id == paperId)
        {
            continue;
        }

        // Keyword similarity: how many keyword words appear in both papers.
        int otherWordCount = splitWords(papers[i].keywords, otherWords, MAX_WORDS);
        int commonKeywords = 0;
        for (int a = 0; a < baseWordCount; a++)
        {
            for (int b = 0; b < otherWordCount; b++)
            {
                if (strcmp(baseWords[a], otherWords[b]) == 0)
                {
                    commonKeywords++;
                    break;
                }
            }
        }

        // Author similarity: how many authors wrote both papers.
        int otherAuthorCount = splitPhrases(papers[i].authors, otherAuthors, 30);
        int commonAuthors = 0;
        for (int a = 0; a < baseAuthorCount; a++)
        {
            for (int b = 0; b < otherAuthorCount; b++)
            {
                if (strcmp(baseAuthors[a], otherAuthors[b]) == 0)
                {
                    commonAuthors++;
                    break;
                }
            }
        }

        if (commonKeywords == 0 && commonAuthors == 0)
        {
            continue;                                  // not similar at all
        }

        all[found].paperId = papers[i].id;
        all[found].commonKeywords = commonKeywords;
        all[found].commonAuthors = commonAuthors;
        all[found].score = commonKeywords * 3 + commonAuthors * 5
                         + (int)papers[i].impactFactor + quartileBonus(papers[i].quartile);
        found++;
    }

    // Selection sort: highest score first.
    for (int i = 0; i < found - 1; i++)
    {
        int best = i;
        for (int j = i + 1; j < found; j++)
        {
            if (all[j].score > all[best].score)
            {
                best = j;
            }
        }
        Recommendation temp = all[i];
        all[i] = all[best];
        all[best] = temp;
    }

    int count = found < maxResults ? found : maxResults;
    for (int i = 0; i < count; i++)
    {
        results[i] = all[i];
    }
    delete[] all;
    return count;
}

History& SearchEngine::getHistory()
{
    return history;
}
