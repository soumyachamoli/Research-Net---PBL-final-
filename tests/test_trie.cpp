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

static void testTextEdgeCases()
{
    cout << "TextUtils edge cases" << endl;
    CHECK(tokenize("").empty());
    CHECK(tokenize("!!! --- ...").empty());
    CHECK(analyze("the of and a").empty());
    CHECK(tokenize("MÃ¼ller lab").at(0) == "mÃ¼ller");
    CHECK(stem("string") == "string");
    CHECK(stem("bus") == "bus");
    CHECK(stem("is") == "is");
    CHECK(normalizeKey("   ").empty());
    CHECK(normalizeKey("A  --  B") == "a b");
    CHECK(containsIgnoreCase("anything", ""));
    CHECK(!containsIgnoreCase("graph", "graphs"));
}

static void testTrie()
{
    cout << "Trie" << endl;
    Trie trie;
    trie.insert("Machine Learning", "keyword", 5);
    trie.insert("machine learning", "keyword", 5);
    trie.insert("Machine Translation", "keyword", 2);
    trie.insert("Machine Vision", "keyword", 8);
    trie.insert("Deep Learning", "keyword", 1);
    trie.insert("Attention Is All You Need", "paper", 3, 42);
    trie.finalize();

    CHECK(trie.size() == 5);
    CHECK(trie.contains("MACHINE LEARNING"));
    CHECK(!trie.contains("machine"));

    vector<Suggestion> s = trie.autocomplete("mach", 10);
    CHECK(s.size() == 3);
    CHECK(s[0].text == "Machine Learning" && s[0].weight == 10);
    CHECK(s[1].text == "Machine Vision");
    CHECK(s[2].text == "Machine Translation");

    CHECK(trie.autocomplete("mach", 1).size() == 1);
    CHECK(trie.autocomplete("MaChInE l", 10).size() == 1);
    CHECK(trie.autocomplete("xyz", 10).empty());
    CHECK(trie.autocomplete("", 10).empty());
    vector<Suggestion> paper = trie.autocomplete("attention", 5);
    CHECK(paper.size() == 1 && paper[0].kind == "paper" && paper[0].paperId == 42);
}

static void testTrieTopSuggestions()
{
    cout << "Trie top suggestions" << endl;
    Trie trie;
    for (int i = 1; i <= 15; i++)
    {
        trie.insert("network " + to_string(i), "keyword", i);
    }
    string longTitle = "A very long paper title that keeps going well past eighty characters so it gets cut";
    trie.insert(longTitle, "paper", 1, 7);
    trie.finalize();

    vector<Suggestion> s = trie.autocomplete("netw", 20);
    CHECK(s.size() == Trie::TOP_K);
    CHECK(s.front().text == "network 15");
    CHECK(s.back().text == "network 6");
    for (size_t i = 1; i < s.size(); i++)
    {
        CHECK(s[i - 1].weight >= s[i].weight);
    }
    vector<Suggestion> paper = trie.autocomplete("a very long paper", 5);
    CHECK(paper.size() == 1 && paper[0].text == longTitle && paper[0].paperId == 7);
}

int main()
{
    testTextUtils();
    testTextEdgeCases();
    testTrie();
    testTrieTopSuggestions();
    return finish();
}
