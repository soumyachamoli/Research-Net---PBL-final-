#include <cstdlib>
#include <cstring>
#include <iostream>
#include "Database.h"
#include "SearchEngine.h"
#include "TextUtils.h"
#include "WebServer.h"
using namespace std;

// Reads one line from the keyboard into buffer. Returns false at end of input.
static bool readLine(char* buffer, int size)
{
    cin.getline(buffer, size);
    if (cin.eof())
    {
        return false;
    }
    if (cin.fail())                  // line was too long: drop the rest
    {
        cin.clear();
        cin.ignore(10000, '\n');
    }
    return true;
}

static void printShort(const Paper& paper)
{
    cout << "  [" << paper.id << "] " << paper.title << endl;
    cout << "       " << paper.year;
    if (paper.quartile[0] != '\0') cout << " | " << paper.quartile;
    if (paper.impactFactor > 0) cout << " | IF: " << paper.impactFactor;
    cout << " | " << paper.journal << endl;
}

static void printPaper(const Paper& paper)
{
    cout << "\nTitle:         " << paper.title << endl;
    cout << "Authors:       " << paper.authors << endl;
    cout << "Journal:       " << paper.journal << endl;
    cout << "Year:          " << paper.year << endl;
    cout << "Impact factor: " << paper.impactFactor << endl;
    cout << "Quartile:      " << (paper.quartile[0] ? paper.quartile : "-") << endl;
    cout << "Keywords:      " << paper.keywords << endl;
    cout << "Abstract:      " << paper.abstractText << endl;
}

static void showResults(SearchEngine& engine, const char* query, const SearchFilter& filter)
{
    int ids[200];
    int count = engine.searchPapers(query, filter, ids, 200);
    cout << "\n" << count << " paper(s) found:" << endl;
    for (int i = 0; i < count; i++)
    {
        printShort(*engine.findPaper(ids[i]));
    }
}

int main(int argc, char* argv[])
{
    Database db;
    if (!db.connect("config.txt"))
    {
        return 1;
    }

    SearchEngine engine;
    if (!engine.load(db))                     // PostgreSQL -> Paper array -> Trie + Inverted Index
    {
        return 1;
    }
    cout << "Loaded " << engine.getPaperCount() << " papers from PostgreSQL ("
         << engine.getWordCount() << " words indexed)." << endl;

    WebServer website(engine, db);
    if (argc > 1 && strcmp(argv[1], "--web") == 0)
    {
        website.start(8080);
        return 0;
    }

    char input[300];
    while (true)
    {
        cout << "\n================================" << endl;
        cout << "          RESEARCHNET" << endl;
        cout << "================================" << endl;
        cout << "1. Search Research Papers" << endl;
        cout << "2. Filter / Sort Papers" << endl;
        cout << "3. Autocomplete" << endl;
        cout << "4. View Paper" << endl;
        cout << "5. Get Recommendations" << endl;
        cout << "6. My History" << endl;
        cout << "7. Start Website" << endl;
        cout << "8. Exit" << endl;
        cout << "\nEnter choice: ";
        if (!readLine(input, sizeof(input)))
        {
            break;
        }
        int choice = atoi(input);
        SearchFilter filter;
        clearFilter(filter);

        if (choice == 1)
        {
            cout << "Search (you can use AND / OR): ";
            readLine(input, sizeof(input));
            showResults(engine, input, filter);
        }
        else if (choice == 2)
        {
            char text[300];
            cout << "Search words (Enter = all papers): ";
            readLine(text, sizeof(text));
            cout << "Year from (Enter = any): ";
            readLine(input, sizeof(input));
            filter.yearFrom = atoi(input);
            cout << "Year to (Enter = any): ";
            readLine(input, sizeof(input));
            filter.yearTo = atoi(input);
            cout << "Author name contains (Enter = any): ";
            readLine(filter.author, sizeof(filter.author));
            cout << "Quartile Q1/Q2/Q3/Q4 (Enter = any): ";
            readLine(input, sizeof(input));
            copyText(filter.quartile, input, QUARTILE_SIZE);
            if (filter.quartile[0] == 'q') filter.quartile[0] = 'Q';
            cout << "Minimum impact factor (Enter = any): ";
            readLine(input, sizeof(input));
            filter.minImpact = (float)atof(input);
            cout << "Sort: 0 = default, 1 = impact factor high-low, 2 = low-high, 3 = newest: ";
            readLine(input, sizeof(input));
            filter.sortBy = atoi(input);
            showResults(engine, text, filter);
        }
        else if (choice == 3)
        {
            cout << "Type the beginning of a word: ";
            readLine(input, sizeof(input));
            char suggestions[10][PHRASE_SIZE];
            int count = engine.autocomplete(input, suggestions, 10);
            if (count == 0) cout << "No suggestions." << endl;
            for (int i = 0; i < count; i++)
            {
                cout << "  " << (i + 1) << ". " << suggestions[i] << endl;
            }
        }
        else if (choice == 4)
        {
            cout << "Paper ID: ";
            readLine(input, sizeof(input));
            Paper paper;
            if (db.searchPaperById(atoi(input), paper))      // read from PostgreSQL
            {
                printPaper(paper);
                engine.getHistory().addHistory(paper.id);   // add to the history linked list
            }
            else
            {
                cout << "No paper with that ID." << endl;
            }
        }
        else if (choice == 5)
        {
            cout << "Paper ID: ";
            readLine(input, sizeof(input));
            Recommendation recs[5];
            int count = engine.recommend(atoi(input), recs, 5);
            if (count == 0) cout << "No recommendations (check the ID)." << endl;
            for (int i = 0; i < count; i++)
            {
                printShort(*engine.findPaper(recs[i].paperId));
                cout << "       score " << recs[i].score << " (" << recs[i].commonKeywords
                     << " keyword words, " << recs[i].commonAuthors << " authors in common)" << endl;
            }
        }
        else if (choice == 6)
        {
            History& history = engine.getHistory();
            cout << "\nRECENTLY VIEWED (newest first): ";
            history.displayHistory();
            HistoryNode* current = history.getHead();
            while (current != nullptr)
            {
                printShort(*engine.findPaper(current->paperId));
                current = current->next;
            }
            cout << "\nd = delete one, c = clear all, Enter = back: ";
            readLine(input, sizeof(input));
            if (input[0] == 'd')
            {
                cout << "Paper ID to remove: ";
                readLine(input, sizeof(input));
                cout << (history.deleteHistoryItem(atoi(input)) ? "Removed." : "That ID is not in the history.") << endl;
            }
            else if (input[0] == 'c')
            {
                history.clearHistory();
                cout << "History cleared." << endl;
            }
        }
        else if (choice == 7)
        {
            website.start(8080);
        }
        else if (choice == 8)
        {
            break;
        }
        else
        {
            cout << "Please enter a number from 1 to 8." << endl;
        }
    }

    db.disconnect();
    cout << "Goodbye!" << endl;
    return 0;
}
