// test_index.cpp
// Build and run all unit tests:  ./build.sh test
#include "check.h"
#include "InvertedIndex.h"
#include "SearchEngine.h"
#include "TextUtils.h"
#include <cmath>
#include <string>
using namespace std;

static void testInvertedIndex()
{
    cout << "InvertedIndex" << endl;
    InvertedIndex index;
    index.addDocument(0, {{"neural", 2}, {"network", 1}});
    index.addDocument(1, {{"network", 3}, {"protocol", 1}});
    index.addDocument(2, {{"neural", 1}, {"vision", 1}});
    index.finalize();

    CHECK(index.documentCount() == 3);
    CHECK(index.termCount() == 4);
    CHECK(index.documentFrequency(index.termId("network")) == 2);
    CHECK(index.termId("missing") == -1);
    CHECK(index.idf(index.termId("vision")) > index.idf(index.termId("network")));   // rarer = higher

    // Every document vector has length 1, so dot(a, a) == 1.
    for (int d = 0; d < 3; d++)
    {
        CHECK(fabs(dot(index.documentVector(d), index.documentVector(d)) - 1.0) < 1e-5);
    }

    auto both = index.search({"neural", "network"}, true);          // AND
    CHECK(both.size() == 1 && both[0].first == 0);
    auto either = index.search({"neural", "network"}, false);       // OR
    CHECK(either.size() == 3);
    CHECK(either[0].first == 0);                                    // matches both words: ranked first
    CHECK(index.search({"neural", "missing"}, true).empty());
    CHECK(index.search({"neural", "missing"}, false).size() == 2);
    CHECK(index.search({}, false).empty());
}

static Paper makePaper(int id, const string& title, const string& abstractText, int year,
                       vector<int> authorIds, vector<string> keywords, double impact = -1)
{
    Paper p;
    p.id = id;
    p.title = title;
    p.abstractText = abstractText;
    p.year = year;
    p.category = "Artificial Intelligence";
    p.topic = id <= 3 ? "Neural Networks" : "Networking";
    p.authorIds = authorIds;
    for (int a : authorIds)
    {
        p.authors.push_back("Author " + to_string(a));
    }
    p.keywords = keywords;
    if (impact >= 0)
    {
        p.impactFactor = impact;
    }
    return p;
}

static vector<Paper> samplePapers()
{
    // Small hand-written fixtures for the tests only (the real data comes from OpenAlex).
    return {
        makePaper(1, "Convolutional neural networks for image classification",
                  "We train deep convolutional neural networks on large image datasets.", 2015, {1, 2},
                  {"deep learning", "image classification"}, 10),
        makePaper(2, "Residual learning for deep image recognition",
                  "Deep residual networks make very deep convolutional networks trainable for images.", 2016,
                  {2, 3}, {"deep learning", "residual networks"}),
        makePaper(3, "Transformers for image recognition at scale",
                  "Attention based transformer models applied to image patches.", 2021, {4},
                  {"transformers", "image classification"}, 40),
        makePaper(4, "Routing protocols for wireless sensor networks",
                  "Energy efficient routing protocol design for wireless sensor networks.", 2012, {5},
                  {"wireless sensor networks", "routing"}),
        makePaper(5, "Congestion control in data center networks",
                  "TCP congestion control for data center switches and routing.", 2019, {6},
                  {"congestion control", "routing"}),
    };
}

static void testSearchEngine()
{
    cout << "SearchEngine" << endl;
    SearchEngine engine(samplePapers());
    CHECK(engine.papers().size() == 5);
    CHECK(engine.findPaper(3) != nullptr && engine.findPaper(99) == nullptr);
    CHECK(engine.minYear() == 2012 && engine.maxYear() == 2021);
    CHECK(engine.maxImpactFactor() == 40);

    auto hits = engine.search("image networks");                 // AND (topic words count too)
    CHECK(hits.size() == 3);
    for (const auto& hit : hits)
    {
        CHECK(hit.paperId <= 3);                                 // networking papers have no "image"
    }
    auto anyHits = engine.search("transformers OR routing");     // OR
    CHECK(anyHits.size() == 3);
    CHECK(engine.search("Transformer").size() == 1);             // stemming + case-insensitive
    CHECK(engine.search("the of and").empty());                  // only stop words
    auto byAuthor = engine.search("author");                     // author names are indexed
    CHECK(byAuthor.size() == 5);

    auto suggestions = engine.autocomplete("ima", 10);
    CHECK(!suggestions.empty() && suggestions[0].text == "image classification");
    CHECK(!engine.autocomplete("author 2", 5).empty());          // author of two papers
    CHECK(engine.autocomplete("author 4", 5).empty());           // only one paper: not suggested
}

int main()
{
    testInvertedIndex();
    testSearchEngine();
    return finish();
}
