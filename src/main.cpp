#include "ActivityRepository.h"
#include "Config.h"
#include "Database.h"
#include "EngineHolder.h"
#include "PaperRepository.h"
#include "UserRepository.h"
#include "WebServer.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
using namespace std;

static shared_ptr<const SearchEngine> buildEngine(PaperRepository& papers)
{
    auto started = chrono::steady_clock::now();
    auto engine = make_shared<const SearchEngine>(papers.loadAll());
    auto ms = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - started).count();
    cout << "Indexed " << engine->papers().size() << " papers: " << engine->index().termCount()
         << " terms, " << engine->trieNodes() << " trie nodes (" << ms << " ms)" << endl;
    return engine;
}

static void watchForChanges(Database& db, EngineHolder& engines, int seconds)
{
    PaperRepository papers(db);
    while (true)
    {
        this_thread::sleep_for(chrono::seconds(seconds));
        try
        {
            string version = papers.dataVersion();
            if (version != engines.version())
            {
                cout << "Papers changed in PostgreSQL, rebuilding the index..." << endl;
                engines.set(buildEngine(papers), version);
            }
        }
        catch (const exception& e)
        {
            cerr << "Index refresh failed (will retry): " << e.what() << endl;
        }
    }
}

static void cleanUpEveryHour(Database& db, int historyDays)
{
    ActivityRepository activity(db);
    UserRepository users(db);
    while (true)
    {
        try
        {
            long sessions = users.deleteExpiredSessions();
            long history = historyDays > 0 ? activity.deleteHistoryOlderThan(historyDays) : 0;
            if (sessions + history > 0)
            {
                cout << "Cleanup: removed " << sessions << " expired sessions and " << history
                     << " history entries older than " << historyDays << " days" << endl;
            }
        }
        catch (const exception& e)
        {
            cerr << "Cleanup failed (will retry): " << e.what() << endl;
        }
        this_thread::sleep_for(chrono::hours(1));
    }
}

int main(int argc, char* argv[])
{
    try
    {
        Config config = loadConfig();
        Database db(config.databaseUrl, config.poolSize);
        PaperRepository papers(db);

        EngineHolder engines;
        string version = papers.dataVersion();
        engines.set(buildEngine(papers), version);
        if (engines.get()->papers().empty())
        {
            cout << "The papers table is empty. Import data with: python tools/load_papers.py" << endl;
        }

        if (argc > 1 && strcmp(argv[1], "--check") == 0)
        {
            return 0;
        }

        if (config.reloadSeconds > 0)
        {
            thread(watchForChanges, ref(db), ref(engines), config.reloadSeconds).detach();
        }
        thread(cleanUpEveryHour, ref(db), config.historyDays).detach();
        WebServer server(config, db, engines);
        return server.start() ? 0 : 1;
    }
    catch (const exception& e)
    {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}
