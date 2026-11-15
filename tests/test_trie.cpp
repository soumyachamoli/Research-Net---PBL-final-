// test_trie.cpp
// Build and run all unit tests:  ./build.sh test
#include "check.h"
#include "TextUtils.h"
#include "Trie.h"
#include <cmath>
#include <string>
using namespace std;

static void testTextUtils()
{
    cout << "TextUtils" << endl;
    vector<string> words = tokenize("Deep-Learning for GPT4 & 5G networks!");
    CHECK(words.size() == 6);
    CHECK(words[0] == "deep" && words[1] == "learning" && words[3] == "gpt4" && words[4] == "5g");
    CHECK(isStopWord("the") && isStopWord("x") && !isStopWord("neural"));
    CHECK(stem("networks") == "network");
    CHECK(stem("studies") == "study");
    CHECK(stem("learning") == "learn");
    CHECK(stem("analysis") == "analysis");
    CHECK(stem("class") == "class");
    CHECK(stem("transformers") == stem("transformer"));
    vector<string> terms = analyze("The networks of the studies");
    CHECK(terms.size() == 2 && terms[0] == "network" && terms[1] == "study");
    CHECK(normalizeKey("  Machine-Learning  ") == "machine learning");
    CHECK(containsIgnoreCase("Ashish Vaswani", "VASWANI"));
}

static void testTrie()
{
    cout << "Trie" << endl;
    Trie trie;
    trie.insert("Machine Learning", "keyword", 5);
    trie.insert("machine learning", "keyword", 5);      // duplicate (different case)
    trie.insert("Machine Translation", "keyword", 2);
    trie.insert("Machine Vision", "keyword", 8);
    trie.insert("Deep Learning", "keyword", 1);
    trie.insert("Attention Is All You Need", "paper", 3, 42);
    trie.finalize();

    CHECK(trie.size() == 5);                           // duplicate was merged
    CHECK(trie.contains("MACHINE LEARNING"));
    CHECK(!trie.contains("machine"));                  // a prefix is not a stored phrase

    vector<Suggestion> s = trie.autocomplete("mach", 10);
    CHECK(s.size() == 3);
    CHECK(s[0].text == "Machine Learning" && s[0].weight == 10);   // 5 + 5
    CHECK(s[1].text == "Machine Vision");
    CHECK(s[2].text == "Machine Translation");

    CHECK(trie.autocomplete("mach", 1).size() == 1);   // maxResults respected
    CHECK(trie.autocomplete("MaChInE l", 10).size() == 1);
    CHECK(trie.autocomplete("xyz", 10).empty());
    CHECK(trie.autocomplete("", 10).empty());
    vector<Suggestion> paper = trie.autocomplete("attention", 5);
    CHECK(paper.size() == 1 && paper[0].kind == "paper" && paper[0].paperId == 42);
}

int main()
{
    testTextUtils();
    testTrie();
    return finish();
}
