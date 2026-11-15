#include "Recommender.h"
#include "TextUtils.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
using namespace std;

static const double MIN_CONTENT = 0.05;   // below this, texts are treated as unrelated

bool RecommenderWeights::parse(const string& text)
{
    vector<double> values;
    stringstream stream(text);
    string part;
    while (getline(stream, part, ','))
    {
        try
        {
            values.push_back(stod(part));
        }
        catch (...)
        {
            return false;
        }
    }
    if (values.size() != 5)
    {
        return false;
    }
    for (double v : values)
    {
        if (v < 0)
        {
            return false;
        }
    }
    content = values[0];
    keywords = values[1];
    authors = values[2];
    recency = values[3];
    impact = values[4];
    return true;
}

Recommender::Recommender(const SearchEngine& engine, RecommenderWeights weights)
    : engine_(engine), weights_(weights)
{
}

// Newest paper in the collection -> 1, oldest -> 0.
double Recommender::recencyScore(const Paper& paper) const
{
    int span = engine_.maxYear() - engine_.minYear();
    if (paper.year == 0 || span <= 0)
    {
        return 0.5;
    }
    return (double)(paper.year - engine_.minYear()) / span;
}

// Log scale, so a journal with impact 40 is not "4 times better" than one with impact 10.
// Returns -1 when the impact factor is unknown.
double Recommender::impactScore(const Paper& paper) const
{
    if (!paper.impactFactor || engine_.maxImpactFactor() <= 0)
    {
        return -1;
    }
    return log1p(*paper.impactFactor) / log1p(engine_.maxImpactFactor());
}

double Recommender::combine(Recommendation& rec) const
{
    double sum = weights_.content * rec.content + weights_.keywords * rec.keywords +
                 weights_.authors * rec.authors + weights_.recency * rec.recency;
    double totalWeight = weights_.content + weights_.keywords + weights_.authors + weights_.recency;
    if (rec.impact >= 0)
    {
        sum += weights_.impact * rec.impact;
        totalWeight += weights_.impact;
    }
    rec.score = totalWeight > 0 ? sum / totalWeight : 0;
    return rec.score;
}

// Normalizes the components over this set of candidates, combines them, keeps the best.
// Raw cosine values are small (0.1-0.4 for related papers) while recency is spread over
// 0..1, so without this step the small-weight recency term could outweigh content.
// Content, keyword and author scores are therefore divided by their maximum here: the
// best match of THIS request gets 1. Recency and impact are already on a 0..1 scale.
vector<Recommendation> Recommender::rank(vector<Recommendation>& candidates, size_t count) const
{
    double maxContent = 0, maxKeywords = 0, maxAuthors = 0;
    for (const Recommendation& rec : candidates)
    {
        maxContent = max(maxContent, rec.content);
        maxKeywords = max(maxKeywords, rec.keywords);
        maxAuthors = max(maxAuthors, rec.authors);
    }
    for (Recommendation& rec : candidates)
    {
        rec.content = maxContent > 0 ? rec.content / maxContent : 0;
        rec.keywords = maxKeywords > 0 ? rec.keywords / maxKeywords : 0;
        rec.authors = maxAuthors > 0 ? rec.authors / maxAuthors : 0;
        combine(rec);
    }
    return topK(candidates, count);
}

// Keeps the `count` best recommendations using a min-heap of size `count`:
// the heap top is the weakest of the current best, and is replaced when a better one comes.
vector<Recommendation> Recommender::topK(vector<Recommendation>& candidates, size_t count) const
{
    auto weaker = [&](int a, int b) { return candidates[a].score > candidates[b].score; };
    priority_queue<int, vector<int>, decltype(weaker)> heap(weaker);
    for (int i = 0; i < (int)candidates.size(); i++)
    {
        heap.push(i);
        if (heap.size() > count)
        {
            heap.pop();
        }
    }
    vector<Recommendation> best;
    while (!heap.empty())
    {
        best.push_back(candidates[heap.top()]);
        heap.pop();
    }
    reverse(best.begin(), best.end());                 // highest score first
    return best;
}

static set<string> lowerSet(const vector<string>& items)
{
    set<string> result;
    for (const string& item : items)
    {
        result.insert(toLowerCopy(item));
    }
    return result;
}

static string joinFirst(const vector<string>& items, size_t limit)
{
    string text;
    for (size_t i = 0; i < items.size() && i < limit; i++)
    {
        text += (i > 0 ? ", " : "") + items[i];
    }
    return text;
}

// Content similarity of `base` to every other paper, using the inverted index: only papers
// that share at least one term are visited. Returns position -> cosine similarity.
static unordered_map<int, double> contentScores(const SearchEngine& engine, const SparseVector& base)
{
    unordered_map<int, double> scores;
    for (const auto& entry : base)
    {
        for (const Posting& posting : engine.index().postings(entry.first))
        {
            scores[posting.doc] += (double)entry.second * posting.weight;
        }
    }
    return scores;
}

vector<Recommendation> Recommender::similarTo(int paperId, size_t count) const
{
    int basePos = engine_.findIndex(paperId);
    if (basePos == -1)
    {
        return {};
    }
    const Paper& base = engine_.papers()[basePos];
    unordered_map<int, double> content = contentScores(engine_, engine_.index().documentVector(basePos));

    // Papers by the same authors or with the same keywords are candidates even if
    // their text happens to be different.
    for (int authorId : base.authorIds)
    {
        for (int pos : engine_.papersByAuthor(authorId))
        {
            content.emplace(pos, 0.0);
        }
    }
    for (const string& keyword : base.keywords)
    {
        for (int pos : engine_.papersWithKeyword(toLowerCopy(keyword)))
        {
            content.emplace(pos, 0.0);
        }
    }

    set<string> baseKeywords = lowerSet(base.keywords);
    unordered_set<int> baseAuthors(base.authorIds.begin(), base.authorIds.end());

    vector<Recommendation> candidates;
    for (const auto& item : content)
    {
        if (item.first == basePos)
        {
            continue;                                  // never recommend the paper itself
        }
        const Paper& other = engine_.papers()[item.first];

        vector<string> sharedKeywords;
        for (const string& keyword : other.keywords)
        {
            if (baseKeywords.count(toLowerCopy(keyword)))
            {
                sharedKeywords.push_back(keyword);
            }
        }
        size_t keywordUnion = baseKeywords.size() + other.keywords.size() - sharedKeywords.size();

        vector<string> sharedAuthors;
        for (size_t a = 0; a < other.authorIds.size(); a++)
        {
            if (baseAuthors.count(other.authorIds[a]))
            {
                sharedAuthors.push_back(other.authors[a]);
            }
        }
        size_t smallerTeam = min(base.authorIds.size(), other.authorIds.size());

        Recommendation rec;
        rec.paperId = other.id;
        rec.content = min(1.0, item.second);
        rec.similarity = rec.content;
        rec.keywords = keywordUnion > 0 ? (double)sharedKeywords.size() / keywordUnion : 0;   // Jaccard
        rec.authors = smallerTeam > 0 ? min(1.0, (double)sharedAuthors.size() / smallerTeam) : 0;
        rec.recency = recencyScore(other);
        rec.impact = impactScore(other);
        if (rec.content < MIN_CONTENT && sharedKeywords.empty() && sharedAuthors.empty())
        {
            continue;                                  // not related at all
        }

        if (!sharedAuthors.empty())
        {
            rec.reasons.push_back("Same author: " + joinFirst(sharedAuthors, 2));
        }
        if (!sharedKeywords.empty())
        {
            rec.reasons.push_back("Similar research keywords: " + joinFirst(sharedKeywords, 3));
        }
        if (rec.content >= 0.25)
        {
            rec.reasons.push_back("Related title and abstract content");
        }
        if (!other.topic.empty() && other.topic == base.topic)
        {
            rec.reasons.push_back("Same research topic: " + other.topic);
        }
        else if (!other.category.empty() && other.category == base.category)
        {
            rec.reasons.push_back("Related research area: " + other.category);
        }
        candidates.push_back(rec);
    }
    return rank(candidates, count);
}

vector<Recommendation> Recommender::forUser(const UserProfile& profile, size_t count) const
{
    const InvertedIndex& index = engine_.index();

    // 1. Build the user's interest vector: a weighted sum of TF-IDF vectors.
    unordered_map<int, double> profileTerms;
    vector<int> sourcePositions;                       // saved first, then viewed
    unordered_set<int> exclude;
    auto addVector = [&](const SparseVector& terms, double weight) {
        for (const auto& entry : terms)
        {
            profileTerms[entry.first] += weight * entry.second;
        }
    };
    for (int id : profile.savedPaperIds)
    {
        int pos = engine_.findIndex(id);
        if (pos != -1 && exclude.insert(pos).second)
        {
            addVector(index.documentVector(pos), 1.0);
            sourcePositions.push_back(pos);
        }
    }
    size_t savedCount = sourcePositions.size();
    for (int id : profile.viewedPaperIds)
    {
        int pos = engine_.findIndex(id);
        if (pos != -1 && exclude.insert(pos).second)
        {
            addVector(index.documentVector(pos), 0.5);
            sourcePositions.push_back(pos);
        }
    }
    for (const string& interest : profile.interests)
    {
        addVector(index.queryVector(analyze(interest)), 0.8);
    }
    for (const string& query : profile.searches)
    {
        addVector(index.queryVector(analyze(query)), 0.4);
    }

    vector<Recommendation> candidates;

    // Nothing known about the user yet: recommend well-cited, recent papers.
    if (profileTerms.empty())
    {
        double maxCites = 1;
        for (const Paper& paper : engine_.papers())
        {
            maxCites = max(maxCites, (double)paper.citedBy);
        }
        for (const Paper& paper : engine_.papers())
        {
            Recommendation rec;
            rec.paperId = paper.id;
            rec.recency = recencyScore(paper);
            rec.impact = impactScore(paper);
            rec.content = log1p(paper.citedBy) / log1p(maxCites);   // popularity stands in for content
            rec.score = 0.7 * rec.content + 0.3 * rec.recency;
            rec.reasons.push_back("Highly cited in " + (paper.category.empty() ? "its field" : paper.category));
            candidates.push_back(rec);
        }
        return topK(candidates, count);
    }

    // Keep the 60 strongest terms (enough to describe the interests, fast to search).
    SparseVector profileVector(profileTerms.begin(), profileTerms.end());
    sort(profileVector.begin(), profileVector.end(),
         [](const pair<int, float>& a, const pair<int, float>& b) { return a.second > b.second; });
    if (profileVector.size() > 60)
    {
        profileVector.resize(60);
    }
    double length = 0;
    for (const auto& entry : profileVector)
    {
        length += (double)entry.second * entry.second;
    }
    length = sqrt(length);
    for (auto& entry : profileVector)
    {
        entry.second = (float)(entry.second / length);
    }
    sort(profileVector.begin(), profileVector.end());

    // Keywords and authors of the papers the user saved or opened.
    set<string> userKeywords;
    unordered_map<int, string> userAuthors;
    for (int pos : sourcePositions)
    {
        const Paper& paper = engine_.papers()[pos];
        for (const string& keyword : paper.keywords)
        {
            userKeywords.insert(toLowerCopy(keyword));
        }
        for (size_t a = 0; a < paper.authorIds.size(); a++)
        {
            userAuthors[paper.authorIds[a]] = paper.authors[a];
        }
    }
    // true if the document contains every analyzed term of a phrase.
    auto containsAll = [&](const SparseVector& docVector, const string& phrase) {
        vector<string> terms = analyze(phrase);
        for (const string& term : terms)
        {
            int id = index.termId(term);
            bool present = false;
            for (const auto& entry : docVector)
            {
                present = present || entry.first == id;
            }
            if (!present)
            {
                return false;
            }
        }
        return !terms.empty();
    };

    unordered_map<int, double> content = contentScores(engine_, profileVector);
    for (const auto& item : content)
    {
        if (exclude.count(item.first))
        {
            continue;                                  // already saved or opened
        }
        const Paper& paper = engine_.papers()[item.first];
        const SparseVector& docVector = index.documentVector(item.first);

        vector<string> sharedKeywords;
        for (const string& keyword : paper.keywords)
        {
            if (userKeywords.count(toLowerCopy(keyword)))
            {
                sharedKeywords.push_back(keyword);
            }
        }
        vector<string> sharedAuthors;
        for (int authorId : paper.authorIds)
        {
            auto found = userAuthors.find(authorId);
            if (found != userAuthors.end())
            {
                sharedAuthors.push_back(found->second);
            }
        }

        Recommendation rec;
        rec.paperId = paper.id;
        rec.content = min(1.0, item.second);
        rec.similarity = rec.content;
        rec.keywords = paper.keywords.empty() ? 0 : (double)sharedKeywords.size() / paper.keywords.size();
        rec.authors = min(1.0, sharedAuthors.size() / 2.0);
        rec.recency = recencyScore(paper);
        rec.impact = impactScore(paper);
        if (rec.content < MIN_CONTENT && sharedKeywords.empty() && sharedAuthors.empty())
        {
            continue;
        }

        // Explanations: which of the user's own papers / interests this matches.
        int bestSource = -1;
        double bestSimilarity = 0.15;                  // only mention clearly similar papers
        for (size_t s = 0; s < sourcePositions.size() && s < 20; s++)
        {
            double similarity = dot(docVector, index.documentVector(sourcePositions[s]));
            if (similarity > bestSimilarity)
            {
                bestSimilarity = similarity;
                bestSource = (int)s;
            }
        }
        if (bestSource != -1)
        {
            const string& title = engine_.papers()[sourcePositions[bestSource]].title;
            rec.reasons.push_back(((size_t)bestSource < savedCount ? "Similar to your saved paper: "
                                                                   : "Similar to a paper you viewed: ") + title);
        }
        for (const string& interest : profile.interests)
        {
            if (containsAll(docVector, interest))
            {
                rec.reasons.push_back("Matches your interest: " + interest);
                break;
            }
        }
        for (const string& query : profile.searches)
        {
            if (containsAll(docVector, query))
            {
                rec.reasons.push_back("Matches your search: " + query);
                break;
            }
        }
        if (!sharedAuthors.empty())
        {
            rec.reasons.push_back("By an author you follow: " + joinFirst(sharedAuthors, 2));
        }
        if (!sharedKeywords.empty())
        {
            rec.reasons.push_back("Similar research keywords: " + joinFirst(sharedKeywords, 3));
        }
        if (rec.reasons.empty())
        {
            rec.reasons.push_back("Related to your saved papers, searches and interests");
        }
        candidates.push_back(rec);
    }
    return rank(candidates, count);
}
