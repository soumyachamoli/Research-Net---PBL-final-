#include "SearchEngine.h"
#include "TextUtils.h"
#include <algorithm>
#include <cmath>
using namespace std;

// How much a word counts depending on where it appears: a word in the title says more
// about a paper than the same word somewhere in the abstract.
static const float TITLE_WEIGHT = 3.0f;
static const float KEYWORD_WEIGHT = 2.0f;
static const float TOPIC_WEIGHT = 2.0f;
static const float AUTHOR_WEIGHT = 2.0f;
static const float JOURNAL_WEIGHT = 1.0f;
static const float CATEGORY_WEIGHT = 1.0f;
static const float ABSTRACT_WEIGHT = 1.0f;

static void addTerms(unordered_map<string, float>& counts, const string& text, float weight)
{
    for (const string& term : analyze(text))
    {
        counts[term] += weight;
    }
}

SearchEngine::SearchEngine(vector<Paper> papers) : papers_(std::move(papers))
{
    for (int i = 0; i < (int)papers_.size(); i++)
    {
        const Paper& paper = papers_[i];
        positionById_[paper.id] = i;
        addToIndex(i, paper);
        addToTrie(paper);

        if (paper.year > 0)
        {
            minYear_ = minYear_ == 0 ? paper.year : min(minYear_, paper.year);
            maxYear_ = max(maxYear_, paper.year);
        }
        if (paper.impactFactor)
        {
            maxImpact_ = max(maxImpact_, *paper.impactFactor);
        }
    }
    index_.finalize();

    // Authors who wrote at least two papers become autocomplete suggestions too.
    for (const auto& item : papersByAuthor_)
    {
        if (item.second.size() >= 2)
        {
            const Paper& paper = papers_[item.second.front()];
            for (size_t a = 0; a < paper.authorIds.size(); a++)
            {
                if (paper.authorIds[a] == item.first)
                {
                    trie_.insert(paper.authors[a], "author", (double)item.second.size());
                }
            }
        }
    }
    trie_.finalize();
}

// Puts one paper's words into the Inverted Index, plus the author/keyword maps.
void SearchEngine::addToIndex(int position, const Paper& paper)
{
    unordered_map<string, float> counts;
    addTerms(counts, paper.title, TITLE_WEIGHT);
    addTerms(counts, paper.abstractText, ABSTRACT_WEIGHT);
    addTerms(counts, paper.topic, TOPIC_WEIGHT);
    addTerms(counts, paper.category, CATEGORY_WEIGHT);
    addTerms(counts, paper.journal, JOURNAL_WEIGHT);
    for (const string& keyword : paper.keywords)
    {
        addTerms(counts, keyword, KEYWORD_WEIGHT);
        papersByKeyword_[toLowerCopy(keyword)].push_back(position);
    }
    for (const string& author : paper.authors)
    {
        addTerms(counts, author, AUTHOR_WEIGHT);
    }
    for (int authorId : paper.authorIds)
    {
        papersByAuthor_[authorId].push_back(position);
    }
    index_.addDocument(position, counts);
}

// Titles, keyword phrases, topics and categories become autocomplete suggestions.
void SearchEngine::addToTrie(const Paper& paper)
{
    // Highly cited papers are suggested first (log scale so one famous paper does not
    // push everything else away).
    trie_.insert(paper.title, "paper", 1.0 + log10(1.0 + paper.citedBy), paper.id);
    for (const string& keyword : paper.keywords)
    {
        trie_.insert(keyword, "keyword", 1.0);      // weight grows with every paper using it
    }
    trie_.insert(paper.topic, "topic", 1.0);
    trie_.insert(paper.category, "category", 1.0);
}

vector<SearchHit> SearchEngine::search(const string& query) const
{
    vector<string> terms;
    bool useOr = false;
    for (const string& word : tokenize(query))
    {
        if (word == "or")
        {
            useOr = true;
        }
        else if (word != "and" && !isStopWord(word))
        {
            terms.push_back(stem(word));
        }
    }

    vector<SearchHit> hits;
    for (const auto& result : index_.search(terms, !useOr))
    {
        hits.push_back({papers_[result.first].id, result.second});
    }
    return hits;
}

vector<Suggestion> SearchEngine::autocomplete(const string& prefix, size_t maxResults) const
{
    return trie_.autocomplete(prefix, maxResults);
}

int SearchEngine::findIndex(int id) const
{
    auto found = positionById_.find(id);            // hash lookup: O(1) on average
    return found == positionById_.end() ? -1 : found->second;
}

const Paper* SearchEngine::findPaper(int id) const
{
    int position = findIndex(id);
    return position == -1 ? nullptr : &papers_[position];
}

const vector<Paper>& SearchEngine::papers() const
{
    return papers_;
}

const InvertedIndex& SearchEngine::index() const
{
    return index_;
}

const vector<int>& SearchEngine::papersByAuthor(int authorId) const
{
    static const vector<int> none;
    auto found = papersByAuthor_.find(authorId);
    return found == papersByAuthor_.end() ? none : found->second;
}

const vector<int>& SearchEngine::papersWithKeyword(const string& keyword) const
{
    static const vector<int> none;
    auto found = papersByKeyword_.find(keyword);
    return found == papersByKeyword_.end() ? none : found->second;
}

int SearchEngine::minYear() const
{
    return minYear_;
}

int SearchEngine::maxYear() const
{
    return maxYear_;
}

double SearchEngine::maxImpactFactor() const
{
    return maxImpact_;
}

size_t SearchEngine::trieNodes() const
{
    return trie_.nodeCount();
}
