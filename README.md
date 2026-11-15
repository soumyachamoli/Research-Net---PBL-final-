# ResearchNet — Research Paper Search and Recommendation System

A second-year PBL project in C++. Research papers are stored in **PostgreSQL**, loaded into C++, and handled with data structures implemented by hand: a **Trie**, **Linked Lists**, and an **Inverted Index** built as a **hash table**. No STL containers are used.

## Features

- Keyword search with **AND / OR**, and **autocomplete** while typing
- **Filters:** publication year range, author, journal quartile (Q1 to Q4), minimum impact factor
- **Sorting:** by impact factor (high to low or low to high) or newest first
- **Paper page** with full details read from PostgreSQL
- **My History:** recently viewed papers (newest first), delete one entry, or clear all
- **Recommendations** based on keyword similarity, author similarity, impact factor and quartile
- Website (Search, Paper and My History pages) and a console menu

## Architecture

```
PostgreSQL (papers table)
      ↓  libpq  (Database.cpp)
Array of Paper structs
      ↓  text processing (TextUtils.cpp: lowercase, split words, remove stop words)
 ┌────┴──────────────┐
Trie            Inverted Index (hash table → linked lists of paper IDs)
 ↓                   ↓
Autocomplete     Keyword search (AND / OR)
                     ↓
              Search results → Recommendations (common keywords)
                     ↓
         Console menu (main.cpp)  or  Website (WebServer.cpp + web/)
```

PostgreSQL only **stores** the papers. All searching is done by the C++ data structures.

## Files

```
include/                 header files (declarations)
  Paper.h                struct Paper: one row of the papers table
  TextUtils.h            lowercase, split into words, stop words
  Trie.h                 Trie for autocomplete
  LinkedList.h           linked list of paper IDs (inverted index, search results)
  History.h              linked list of viewed papers (browsing history)
  InvertedIndex.h        word → linked list of paper IDs (hash table)
  Database.h             PostgreSQL connection (libpq)
  SearchEngine.h         loads papers, builds Trie + index, search, recommend
  WebServer.h            small web server for the website
src/                     the matching .cpp files + main.cpp (menu)
web/                     index.html, style.css, app.js (the website)
sql/database.sql         creates the papers table + 64 real papers
config.example.txt       database settings template (copy to config.txt)
```

## How each part works

**Trie (`Trie.cpp`).** Each node has `children[27]` (a–z plus space) and `isEndOfWord`. Words that start the same way share nodes. `autocomplete("mach")` walks m→a→c→h, then visits every node below it (recursively) and collects the words it finds: *machine, machine learning, machine translation*. Keyword phrases like "machine learning" are inserted too, which is why the space slot exists.

**Linked List (`LinkedList.cpp`).** A chain of `ListNode {value, next}` storing paper IDs without duplicates. `addAll()` is used for **OR** (union) and `keepOnlyCommon()` for **AND** (intersection).

**Inverted Index (`InvertedIndex.cpp`).** An array of 101 buckets. `hashFunction(word)` turns a word into a bucket number. Each bucket holds a linked list of `WordNode`s (for words that land in the same bucket), and each `WordNode` has a `LinkedList` of the papers containing that word:

```
"learning" → hash → bucket 57 → WordNode "learning" → [2, 10, 28, 34 ...]
```

**History (`History.cpp`).** A linked list of `HistoryNode {paperId, next}`. `addHistory()` puts the paper at the head (newest first) and first removes it if it was already there. `deleteHistoryItem()` finds the node, links the previous node to the next one, and frees the node with `delete`. `clearHistory()` deletes every node. At most 30 entries are kept.

**Filters and sorting (`SearchEngine::searchPapers`).** Step 1 gets candidate papers from the inverted index (or all papers if the search box is empty). Step 2 keeps only papers that pass every filter. Step 3 sorts them with **insertion sort** by impact factor or year.

**Search (`SearchEngine::keywordSearch`).** The query is split into words. The first word's paper list is copied into the results. For each next word, the results are intersected with its list (AND, the default) or united with it (after `OR`).

**Recommendation (`SearchEngine::recommend`).** For every other paper:

```
score = 3 × (shared keyword words) + 5 × (shared authors)
      + impact factor (rounded down) + quartile bonus (Q1 = 4, Q2 = 3, Q3 = 2, Q4 = 1)
```

Only papers that share at least one keyword word or author are considered. They are sorted with **selection sort** and the top 6 are shown, with the reasons.

**Database (`Database.cpp`).** `connect()` reads the connection line from `config.txt`. `getPapers()` runs `SELECT ... FROM papers` and copies each row into a `Paper` struct (in a `new Paper[rows]` array). `searchPaperById()` uses a parameter (`$1`), which protects against SQL injection.

## Time complexity

| Operation | Time | Notes |
|---|---|---|
| Trie insert / search | O(L) | L = length of the word |
| Trie autocomplete | O(P + N) | P = prefix length, N = nodes below the prefix |
| Index insert / lookup | O(1) average | hash to a bucket, then a short bucket list |
| Linked list insert | O(n) | checks for duplicates while walking to the end |
| AND (keepOnlyCommon) | O(n × m) | sizes of the two lists |
| History add / delete | O(n) | n ≤ 30 entries |
| Filter + insertion sort | O(R) + O(R²) | R = search results |
| Recommendation | O(P × K²) + O(P²) sort | P = papers, K = keyword words per paper |

## Run in GitHub Codespaces (nothing to install on your PC)

1. Open the repository → **Code → Codespaces → Create codespace**. `.devcontainer/setup.sh` runs automatically: it installs g++, PostgreSQL and libpq, creates the database and table, inserts the papers, writes `config.txt`, and compiles.
2. Menu version: `./researchnet`
3. Website version: `./researchnet --web`, then click **Open in Browser** for port 8080.

To redo the setup by hand: `bash .devcontainer/setup.sh`

## Run on Windows + VS Code (needs about 1.5 GB of disk space)

1. Install **PostgreSQL** from postgresql.org (keep port 5432, remember your password). This also installs **libpq**: headers in `C:\Program Files\PostgreSQL\<version>\include`, the library in `...\lib`.
2. Install **MSYS2** (msys2.org). In the "MSYS2 UCRT64" terminal run `pacman -S mingw-w64-ucrt-x86_64-gcc`.
3. Add `C:\msys64\ucrt64\bin` and `C:\Program Files\PostgreSQL\<version>\bin` to your PATH, then restart VS Code.
4. Create the database and table:
   ```
   psql -U postgres -c "CREATE DATABASE researchnet;"
   psql -U postgres -d researchnet -f sql/database.sql
   ```
5. Copy `config.example.txt` to `config.txt` and put in your password.
6. Compile (replace `17` with your PostgreSQL version):
   ```
   g++ -std=c++17 -I include -I "C:/Program Files/PostgreSQL/17/include" src/*.cpp -o researchnet.exe -L "C:/Program Files/PostgreSQL/17/lib" -lpq -lws2_32
   ```
   `-I` = where header files are, `-L` = where libraries are, `-lpq` = link libpq, `-lws2_32` = Windows networking (for the website).
7. Run `researchnet.exe`, or `researchnet.exe --web` and open http://localhost:8080.

## Your password

`config.txt` holds the connection line, including your password. It is listed in `.gitignore`, so Git never uploads it. Only `config.example.txt` (with a placeholder) goes to GitHub. To change the password, just edit `config.txt`.

## Data

`sql/database.sql` inserts 100 papers of two kinds:

- **64 real papers.** Titles, authors, years and arXiv links are real; `abstract_text` is a short one-line summary, not the official abstract. They are arXiv preprints, so they have **no impact factor and no quartile**: impact factors belong to journals.
- **36 sample papers** (titles start with `[SAMPLE]`): invented papers in invented journals with **demo** impact factors and quartiles, so the filters and sorting can be tested. Real impact factors come from Clarivate's Journal Citation Reports (paid); free journal quartiles are available from SCImago (scimagojr.com).

## Limitations

- Only letters a–z are indexed (digits and accents are ignored).
- Search results are sorted by ID, impact factor or year, not by relevance.
- History is kept in memory, so it is empty again after restarting the program.
- The website handles one request at a time (no threads), which is fine for one user.
