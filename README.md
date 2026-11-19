# ResearchNet: Research Paper Search and Recommendation

A second-year PBL project. ResearchNet is a website for finding research papers: ranked
keyword search with Trie autocomplete, advanced filters, paper pages, saved papers, history,
and recommendations that explain themselves. The search and recommendation algorithms are
implemented in **C++17**. All data (papers, users, bookmarks, history) lives in
**PostgreSQL**.

It currently holds **10,319 genuine research papers** from [OpenAlex](https://openalex.org)
(CC0 open data): the ~1,000 most-cited English articles and reviews (2010-2025, each with an
abstract and DOI) from ten Computer Science research areas, with 34,007 authors, 1,707
journals/venues and 31,118 keywords. No record is invented.

## Quick start: run it on your own computer

Choose **one** of these. Each gives you the full website with all 10,319 papers.

### Option A: Docker (Windows, Mac or Linux, recommended)

1. Install **Docker Desktop** from https://www.docker.com/products/docker-desktop and start it.
   Wait until it says Docker is running.
2. Download this project. Either click the green **Code** button on GitHub, then
   **Download ZIP**, and unzip it, or run:
   ```
   git clone https://github.com/soumyachamoli/Research-Net---PBL-final-.git
   ```
3. Open a terminal **inside the project folder** (on Windows: open the folder, click
   the address bar, type `cmd` and press Enter) and run:
   ```
   docker compose up --build
   ```
4. The first run takes about 5 minutes: it builds the C++ program and loads the papers.
   When you see `ResearchNet running at http://0.0.0.0:8080`, open
   **http://localhost:8080** in your browser.

To stop it press **Ctrl + C**. Next time, `docker compose up` starts in seconds.
To delete everything (database included): `docker compose down -v`.

If port 8080 is already used on your computer, change `"8080:8080"` in
`docker-compose.yml` to `"8081:8080"` and open http://localhost:8081.

### Option B: GitHub Codespaces (nothing to install, runs in the browser)

1. On the GitHub page, click **Code**, then **Codespaces**, then
   **Create codespace on main** (you need a free GitHub account).
2. Wait about 5 minutes while it installs PostgreSQL, loads the papers and builds.
3. In the terminal at the bottom, run `./researchnet`, then click **Open in Browser**
   when the pop-up appears (or open the **Ports** tab and click the 8080 link).

### Option C: install everything yourself

See [Run it locally](#run-it-locally) below.

### If something goes wrong

| Problem | Fix |
|---|---|
| `Cannot connect to the Docker daemon` / `docker: command not found` | Open Docker Desktop and wait until it says it is running, then try again. |
| `port is already allocated` or `Could not listen on ... 8080` | Something else uses port 8080. Change `"8080:8080"` to `"8081:8080"` in `docker-compose.yml` (Docker) or run `PORT=8081 ./researchnet`, then open port 8081. |
| The page loads slowly or not at all on `localhost` (Windows) | Use `http://127.0.0.1:8080` instead of `localhost`. |
| `Permission denied` when running `./build.sh` | Run `chmod +x build.sh` once, or start it with `bash build.sh`. |
| The compiler runs out of memory | Build one file at a time: `JOBS=1 ./build.sh`. In Docker Desktop, give Docker more memory under Settings, Resources. |
| `libpq.dll was not found` (Windows) | Add PostgreSQL's `bin` folder to `PATH`, e.g. `export PATH="/c/Program Files/PostgreSQL/18/bin:$PATH"`. |
| `password authentication failed` | The password in `config.txt` or `DATABASE_URL` does not match your PostgreSQL password. |
| `The papers table is empty` | Import the data: `python tools/load_papers.py "<connection string>"`. |

## Use of AI tools

This project was built with substantial help from **Claude**, an AI assistant made by
Anthropic. I wrote the first version (v4: a C++ Trie, linked lists and a hash-table
inverted index over papers stored in PostgreSQL, with a console menu and a simple
website) and set the requirements for this version. Claude reviewed v4 and then wrote
most of the current code to those requirements: the STL rewrite of the data structures,
the TF-IDF search and recommender, the REST API, accounts, the website, the OpenAlex
import scripts, the tests and the deployment setup. I ran, tested and reviewed the
result.

## Features

| Area | What it does |
|---|---|
| Search | Ranked keyword search over title, abstract, keywords, topic, author names and journal (TF-IDF). `AND` by default, `OR` supported; falls back to "any word" when no paper has all words, and says so. |
| Autocomplete | Trie suggestions for paper titles, keywords, topics, research categories and authors, ranked and without duplicates. |
| Filters | Author, journal, research category, keyword, publication type, exact year, year range, min/max journal impact. All combine. |
| Sorting | Relevance, newest, oldest, impact high to low / low to high, most cited, first author A-Z. Paginated. |
| Paper page | Full abstract, authors and keywords (clickable filters), DOI and publisher links, journal details with the impact metric's source and year, similar papers. |
| Recommendations | Similar papers for every paper, plus personal recommendations from saved papers, viewed papers, searches and profile interests, each with reasons and a score breakdown. |
| Accounts | Register (name, email, password), login, logout, profile (name, interests, password change). |
| Your data | Saved papers, search history and viewed papers; delete one item or clear all. Private to each user. |

Pages: landing, login/register, dashboard, search, paper details, recommendations, saved
papers, history, profile. The site works on phones (collapsible menu and filters).

## Architecture

```
 Browser (web/: index.html, style.css, app.js - plain JavaScript, hash routing)
     |  JSON over HTTP (REST API)
     v
 C++17 server (cpp-httplib)
     WebServer + PaperRoutes / AuthRoutes / UserRoutes   (HTTP, validation, sessions)
     SearchEngine:  Trie  +  InvertedIndex (TF-IDF)  +  id/author/keyword hash maps
     Recommender:   TF-IDF cosine similarity + weighted scoring + top-k heap
     PaperRepository / UserRepository / ActivityRepository   (all SQL, parameterized)
     Database: pool of libpq connections
     |
     v
 PostgreSQL (papers, authors, journals, keywords, users, sessions, bookmarks, history, ...)
     ^
     |  tools/fetch_openalex.py -> data/openalex_papers.jsonl.gz -> tools/load_papers.py
 OpenAlex API
```

How a search runs:

1. The C++ **inverted index** finds the papers that match the words and scores them (TF-IDF cosine).
2. PostgreSQL receives those ids and scores (`unnest(ids, scores)`) and applies the **filters**,
   **sorting** and **pagination** with its indexes, all in one query.
3. The page of results is returned as JSON. A logged-in user's search goes into their history.

At startup the server reads all papers from PostgreSQL and builds the Trie and index
(about 3-6 seconds for 10,319 papers, about 175 MB of RAM). A background thread checks
every `RELOAD_SECONDS` whether papers changed (row count + latest `updated_at`). If they
did, it builds a new engine and swaps it in while requests keep running.

## How the algorithms work

### Trie (autocomplete): `include/Trie.h`, `src/Trie.cpp`

A prefix tree: every phrase is a path from the root, one character per level, and phrases
with the same beginning share nodes. Keys are normalized (lower case, letters/digits/spaces),
so matching is case-insensitive. Inserting the same phrase twice adds to its weight instead
of creating a duplicate.

- Inserted: paper titles (weight `1 + log10(1 + citations)`), keyword phrases, topics and
  categories (weight = number of papers using them), and authors with 2+ papers.
- Nodes are stored in a `std::vector`; each node keeps its children as `(char, index)` pairs.
- After building, `finalize()` walks the tree once, children before parents, and stores the
  **10 best suggestions of each subtree in its node**. Autocomplete then just walks down
  the typed prefix and returns that list: **O(length of the prefix)**, whatever the size
  of the dataset.
- Size with 10,319 papers: about 930,000 nodes.

### Inverted index (search): `include/InvertedIndex.h`, `src/InvertedIndex.cpp`

A hash table (`std::unordered_map`) from each word to its **posting list**: the papers that
contain it, each with a weight.

1. Text processing (`TextUtils`): lower-case, split into words (digits kept: "5g", "gpt4"),
   remove stop words (`std::unordered_set`), light stemming ("networks" -> "network",
   "studies" -> "study", "learning" -> "learn"). Queries go through the same steps.
2. Word counts per paper are field-weighted: title x3; keywords, topic and authors x2;
   abstract, journal and category x1.
3. Weights: `tf-idf = (1 + ln tf) * idf`, with `idf = ln((N + 1) / (df + 1)) + 1`, where
   `df` is the number of papers containing the word. Rare words count more. Each paper's
   vector is divided by its length, so a dot product equals **cosine similarity**.
4. Query: only the posting lists of the query words are walked, adding up
   `query weight x paper weight` per paper. With AND, a paper must contain every word.
   Results are sorted by score (`std::sort`).

The index holds about 61,000 distinct terms. A search takes about 14 ms end to end
(median, including PostgreSQL filtering).

### Recommendation algorithm: `include/Recommender.h`, `src/Recommender.cpp`

Every candidate gets five scores, each normalized to 0..1:

| Component | Meaning | Default weight |
|---|---|---|
| content | TF-IDF cosine similarity (title, abstract, keywords, topic, ...) | 0.50 |
| keywords | shared keywords (Jaccard overlap) | 0.20 |
| authors | shared authors | 0.15 |
| recency | `(year - oldest) / (newest - oldest)` | 0.10 |
| impact | `log(1 + impact) / log(1 + max impact)`, only when known | 0.05 |

```
score = sum(weight_i * component_i) / sum(weight_i)
```

- Content, keyword and author scores are divided by the best value among the candidates of
  that request, so the best match gets 1. Raw cosines are small (0.1-0.4), and without
  this step the low-weight recency term could outweigh text similarity.
- When the impact is unknown, it is left out of **both** sums: missing data is neither a
  zero nor a penalty, and impact can never dominate relevance.
- **Similar papers**: candidates come from the inverted index (only papers sharing a
  word are visited) plus papers with the same authors or keywords. The paper itself is
  never recommended.
- **For you**: the user's interest vector is a weighted sum of TF-IDF vectors: saved
  papers x1.0, viewed papers x0.5, profile interests x0.8, recent searches x0.4. Papers
  already saved or opened are skipped. New users get recent, highly cited papers.
- Top-k uses a size-k min-heap (`std::priority_queue`).
- Every result lists its reasons, all traceable to data: "Same author: ...", "Similar
  research keywords: ...", "Similar to your saved paper: <title>", "Matches your interest:
  ...", "Same research topic: ...".
- The weights can be changed without recompiling: `RN_WEIGHTS=0.5,0.2,0.15,0.1,0.05`.

### Advanced filters: `src/PaperRepository.cpp`

Each filter adds one SQL condition, and all conditions are combined with AND. User
input is always a parameter (`$1`, `$2`, ...), never pasted into the SQL.

| Filter | SQL | Index used |
|---|---|---|
| author | an author name word starts with the text (`ILIKE`) | trigram GIN on `authors.name` |
| journal | journal name contains the text | trigram GIN on `journals.name` |
| category, type, keyword | exact match | B-tree indexes |
| year / year range | `publication_year = / >= / <=` | B-tree on `publication_year` |
| impact range | `journals.impact_factor >= / <=` | B-tree on `impact_factor` |

Papers with an unknown impact are excluded when an impact filter is set; they are not
treated as 0. Sorting puts unknown values last.

## About "impact factor"

Journal Impact Factors are owned by Clarivate and are not open data. ResearchNet uses
**OpenAlex's 2-year mean citedness** of each venue, an open, impact-factor-like journal
metric. It is stored on the `journals` table together with its source
(`OpenAlex 2yr_mean_citedness`) and the year it was retrieved, and the site labels it
accordingly. Repositories such as arXiv get no value. 10,140 of the 10,319 papers have one.

## Database

`sql/schema.sql` (safe to run again, never deletes data):

- Research data: `papers`, `authors`, `paper_authors`, `journals`, `keywords`, `paper_keywords`
- Users: `users` (bcrypt hash), `sessions` (SHA-256 of the token), `bookmarks`,
  `search_history`, `view_history`, `user_interests`
- Indexes for every filter, a trigram index for name search, and a generated `tsvector`
  column (PostgreSQL full-text search) kept as a baseline for comparison.

## Folder structure

```
include/, src/         C++17 source (one class per file pair)
  TextUtils            tokenizing, stop words, stemming
  Trie                 autocomplete
  InvertedIndex        TF-IDF index, cosine similarity
  SearchEngine         builds Trie + index from the papers, search, lookups
  Recommender          similar papers, personal recommendations
  Database             libpq connection pool, parameterized queries
  PaperRepository      paper SQL: filters, sorting, pagination, details
  UserRepository       accounts, password hashing, sessions
  ActivityRepository   bookmarks, history, interests
  WebServer            server setup, security, sessions, login limiter
  PaperRoutes, AuthRoutes, UserRoutes   REST API handlers
  Config, EngineHolder, HttpUtils, main
web/                   website (index.html, style.css, app.js)
sql/schema.sql         database schema
tools/                 fetch_openalex.py (download + clean), load_papers.py (import)
data/                  openalex_papers.jsonl.gz: the cleaned dataset (10,319 papers)
tests/                 C++ unit tests, api_test.py (end to end), ui_test.mjs (browser)
third_party/           cpp-httplib 0.18.3, nlohmann/json 3.11.3 (single headers, MIT)
Dockerfile, render.yaml, .github/workflows/ci.yml   deployment and CI
```

## Run it locally

You need a C++17 compiler (g++ 8 or newer), PostgreSQL 13+ with libpq, and Python 3 with psycopg.

**1. Database**

```bash
createdb researchnet                                  # or: psql -c "CREATE DATABASE researchnet;"
pip install "psycopg[binary]"
python tools/load_papers.py "postgresql://USER:PASSWORD@localhost:5432/researchnet"
```

The loader creates the tables (`sql/schema.sql`) and imports the papers. Running it again
changes nothing.

**2. Configure.** Either set `DATABASE_URL`, or copy `config.example.txt` to `config.txt`
and put in your password (`config.txt` and `.env` are git-ignored).

**3. Build and run**

```bash
./build.sh test        # unit tests (no database needed)
./build.sh             # builds ./researchnet
./researchnet          # http://127.0.0.1:8080
```

Windows: install PostgreSQL and MSYS2 (`pacman -S mingw-w64-ucrt-x86_64-gcc`), then run
`./build.sh` from Git Bash. It uses `C:/msys64/ucrt64` g++ and links PostgreSQL's
`libpq.dll`; set `PG_HOME` if PostgreSQL is not in `C:/Program Files/PostgreSQL/18`.
Add PostgreSQL's `bin` folder to `PATH` so `libpq.dll` is found at runtime. If the
compiler runs out of memory, use `JOBS=1 ./build.sh`.

**GitHub Codespaces:** open the repository in a codespace. `.devcontainer/setup.sh`
installs everything, imports the papers and builds. Then run `./researchnet`.

### Settings (environment variables)

| Variable | Default | Meaning |
|---|---|---|
| `DATABASE_URL` | `config.txt` | PostgreSQL connection string |
| `HOST`, `PORT` | `127.0.0.1`, `8080` | where the server listens |
| `DB_POOL_SIZE` | 4 | PostgreSQL connections |
| `COOKIE_SECURE` | off | `1` = session cookie only over HTTPS (production) |
| `TRUST_PROXY` | off | `1` = client IP from `X-Forwarded-For` (behind a host's proxy) |
| `RELOAD_SECONDS` | 60 | how often to check for changed papers (0 = never) |
| `SIGNUP_LIMIT_PER_HOUR` | 20 | new accounts allowed per client address per hour |
| `HISTORY_DAYS` | 180 | search and view history older than this is deleted hourly (0 = keep forever) |
| `RN_WEIGHTS` | `0.5,0.2,0.15,0.1,0.05` | recommender weights |

### Refreshing the data

```bash
python tools/fetch_openalex.py --per 1050      # optional: OPENALEX_API_KEY raises the API budget
python tools/load_papers.py "$DATABASE_URL"
```

The fetch script caches raw API pages in `data/raw/` (git-ignored), backs off on rate
limits, and only uses the standard library. A running server picks up the changes by itself.

## REST API

| Method | Path | Notes |
|---|---|---|
| GET | `/api/search`, `/api/papers` | `q, author, journal, category, keyword, type, year, year_from, year_to, min_impact, max_impact, sort, page, page_size` |
| GET | `/api/autocomplete?prefix=` | Trie suggestions |
| GET | `/api/papers/:id` | full details (recorded as viewed when logged in) |
| GET | `/api/papers/:id/similar` | similar papers with scores and reasons |
| GET | `/api/recommendations` | login required |
| GET | `/api/categories`, `/api/filters`, `/api/featured`, `/api/stats`, `/api/health` | |
| POST | `/api/register`, `/api/login`, `/api/logout` | JSON body |
| GET | `/api/me` | current user |
| GET, POST | `/api/bookmarks` | POST `{"paperId": 12}` |
| DELETE | `/api/bookmarks/:paperId` | |
| GET, DELETE | `/api/history` | DELETE clears everything |
| DELETE | `/api/history/searches/:id`, `/api/history/views/:paperId` | |
| GET, PUT | `/api/profile` | PUT `{"name", "interests": [...]}` |
| PUT | `/api/profile/password` | `{"currentPassword", "newPassword"}` |

Errors are JSON (`{"error": "..."}`) with the right status: 400 invalid input, 401 not
logged in, 403 forbidden, 404 not found, 409 duplicate email, 415 not JSON, 429 too many
failed logins.

## Security

- Passwords: bcrypt (cost 10) via PostgreSQL `pgcrypto`, 8-72 characters; never stored or
  logged in plain text.
- Sessions: random 256-bit token in an `HttpOnly`, `SameSite=Lax` cookie (`Secure` in
  production), valid 7 days; only its SHA-256 hash is stored. A password change ends
  other sessions.
- Duplicate accounts are impossible (unique index on `lower(email)`).
- Login: the same error for unknown email and wrong password, and 10 failures per client
  per 15 minutes -> 429.
- Sign-ups: at most 20 new accounts per client address per hour (`SIGNUP_LIMIT_PER_HOUR`) -> 429.
- Requests that change data are rejected if they come from another site (`Origin` check)
  or are not JSON (CSRF protection).
- Every query is parameterized; every user-data query is scoped to the logged-in user.
- The website inserts data as text only, under a strict Content-Security-Policy.

## Tests

| Suite | Command | Checks |
|---|---|---|
| C++ unit tests | `./build.sh test` | 80 (text processing, Trie, inverted index, search engine, recommender) |
| API end to end | `python tests/api_test.py http://127.0.0.1:8080 "$DATABASE_URL"` | 74 (search, every filter and sort, pagination, validation, accounts, bookmarks, history, isolation between users, recommendations, filter counts cross-checked with SQL, latency) |
| Browser | `node tests/ui_test.mjs http://127.0.0.1:8080` (headless Chrome on port 9222) | 28 (all pages, registration, filters, bookmarks, profile, phone layout, no console errors) |

GitHub Actions (`.github/workflows/ci.yml`) runs the unit tests, imports the data into
PostgreSQL, runs the API suite and builds the Docker image on every push.

## Deployment

The server (website + API) runs as one Docker container. PostgreSQL is hosted separately.

1. **Database**: create a PostgreSQL database (e.g. [Neon](https://neon.tech) free tier, or
   Render PostgreSQL) and copy its connection string (with `sslmode=require`).
2. **Load the papers** from your computer: `python tools/load_papers.py "<connection string>"`.
3. **Web service**: on [Render](https://render.com), create a Blueprint from this repository
   (`render.yaml`) and enter `DATABASE_URL` when asked. Render builds the `Dockerfile`,
   checks `/api/health`, and redeploys on every push.

Secrets go only into the hosting dashboard, never into the repository.

## Limitations

- About 10k papers from ten Computer Science areas, chosen as the most cited, so the
  collection is biased toward well-known work. The index is rebuilt in memory, which suits
  tens of thousands of papers, not millions.
- The impact metric is OpenAlex's 2-year mean citedness, not Clarivate's JIF.
- The stemmer is simple (plurals, -ing, -ed). There is no spelling correction and no
  phrase search ("..." quotes).
- Search relevance has not been evaluated against human relevance judgements; a labelled
  query set with precision@k would be the next step.
- Login rate limiting is kept in memory, so it is per server instance.
- On free hosting tiers the server sleeps when idle; the first request then takes longer
  while the index is rebuilt.

## Credits

Paper metadata: [OpenAlex](https://openalex.org) (CC0). HTTP server:
[cpp-httplib](https://github.com/yhirose/cpp-httplib) (MIT). JSON:
[nlohmann/json](https://github.com/nlohmann/json) (MIT).
