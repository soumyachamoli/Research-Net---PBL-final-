// test_recommender.cpp
// Build and run all unit tests:  ./build.sh test
#include "check.h"
#include "Recommender.h"
#include "SearchEngine.h"
#include <algorithm>
#include <cmath>
#include <string>
using namespace std;

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

static void testRecommender()
{
    cout << "Recommender" << endl;
    SearchEngine engine(samplePapers());
    Recommender recommender(engine, RecommenderWeights());

    auto similar = recommender.similarTo(1, 3);
    CHECK(!similar.empty() && similar.size() <= 3);
    for (const auto& r : similar)
    {
        CHECK(r.paperId != 1);                                    // never the paper itself
        CHECK(r.score >= 0 && r.score <= 1);
        CHECK(!r.reasons.empty());
    }
    CHECK(similar[0].paperId == 2);
    double bestContent = 0;
    for (const auto& r : similar)
    {
        bestContent = max(bestContent, r.content);
    }
    CHECK(fabs(bestContent - 1.0) < 1e-9);                        // best match of the request -> 1                               // shares author 2 + keyword + text
    for (size_t i = 1; i < similar.size(); i++)
    {
        CHECK(similar[i - 1].score >= similar[i].score);          // sorted best first
    }
    bool sameAuthorReason = false;
    for (const string& reason : similar[0].reasons)
    {
        sameAuthorReason = sameAuthorReason || reason.find("Same author") == 0;
    }
    CHECK(sameAuthorReason);

    // Unknown impact factor is left out, not counted as zero.
    for (const auto& r : recommender.similarTo(2, 5))
    {
        if (r.paperId == 4 || r.paperId == 5)
        {
            CHECK(r.impact == -1);
        }
    }
    CHECK(recommender.similarTo(999, 5).empty());

    UserProfile profile;
    profile.savedPaperIds = {4};
    auto forUser = recommender.forUser(profile, 5);
    CHECK(!forUser.empty() && forUser[0].paperId == 5);           // the other networking paper
    for (const auto& r : forUser)
    {
        CHECK(r.paperId != 4);                                    // saved papers are skipped
    }

    UserProfile empty;
    auto coldStart = recommender.forUser(empty, 2);
    CHECK(coldStart.size() == 2);

    RecommenderWeights w;
    CHECK(w.parse("0.6,0.1,0.1,0.1,0.1") && w.content == 0.6);
    CHECK(!w.parse("1,2,3"));
    CHECK(!w.parse("1,2,3,4,-5"));
}

int main()
{
    testRecommender();
    return finish();
}
