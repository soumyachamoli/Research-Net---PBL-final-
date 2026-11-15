"""
Load the cleaned OpenAlex dataset into PostgreSQL.

    pip install "psycopg[binary]"
    python tools/load_papers.py "postgresql://user:pass@host:5432/researchnet"
    python tools/load_papers.py            # uses $DATABASE_URL

Repeatable: papers are matched by DOI, journals/authors by OpenAlex id and keywords
by lower-case name, so running it twice does not create duplicates. Rows are only
rewritten (and updated_at bumped) when the data actually changed, which is what the
C++ server watches to know when to rebuild its Trie and inverted index.
"""
import gzip
import json
import os
import sys

import psycopg

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
DATA_FILE = os.path.join(ROOT, "data", "openalex_papers.jsonl.gz")
SCHEMA_FILE = os.path.join(ROOT, "sql", "schema.sql")
BATCH = 500


def read_papers(path):
    with gzip.open(path, "rt", encoding="utf-8") as f:
        return [json.loads(line) for line in f]


def upsert_journals(cur, papers):
    journals = {}
    for p in papers:
        j = p.get("journal")
        if j and j.get("openalex_id") and j.get("name"):
            journals[j["openalex_id"]] = j
    cur.executemany(
        """INSERT INTO journals (openalex_id, name, issn_l, source_type, publisher,
                                 impact_factor, impact_factor_source, impact_factor_year)
           VALUES (%(openalex_id)s, %(name)s, %(issn_l)s, %(type)s, %(publisher)s,
                   %(impact_factor)s, %(impact_factor_source)s, %(impact_factor_year)s)
           ON CONFLICT (openalex_id) DO UPDATE SET
               name = EXCLUDED.name, issn_l = EXCLUDED.issn_l, source_type = EXCLUDED.source_type,
               publisher = EXCLUDED.publisher, impact_factor = EXCLUDED.impact_factor,
               impact_factor_source = EXCLUDED.impact_factor_source,
               impact_factor_year = EXCLUDED.impact_factor_year""",
        list(journals.values()))
    cur.execute("SELECT openalex_id, id FROM journals WHERE openalex_id IS NOT NULL")
    return dict(cur.fetchall())


def upsert_authors(cur, papers):
    authors = {}
    for p in papers:
        for a in p["authors"]:
            if a.get("openalex_id"):
                authors[a["openalex_id"]] = a["name"]
    cur.executemany(
        """INSERT INTO authors (openalex_id, name) VALUES (%s, %s)
           ON CONFLICT (openalex_id) DO UPDATE SET name = EXCLUDED.name
           WHERE authors.name IS DISTINCT FROM EXCLUDED.name""",
        list(authors.items()))
    cur.execute("SELECT openalex_id, id FROM authors WHERE openalex_id IS NOT NULL")
    return dict(cur.fetchall())


def upsert_keywords(cur, papers):
    names = {}
    for p in papers:
        for k in p["keywords"]:
            names.setdefault(k.lower(), k)
    cur.executemany("INSERT INTO keywords (name) VALUES (%s) ON CONFLICT ((lower(name))) DO NOTHING",
                    [(n,) for n in names.values()])
    # Lower-case in Python, not SQL: PostgreSQL's lower() depends on the database locale
    # (with the C locale it leaves letters like "Ł" unchanged).
    cur.execute("SELECT name, id FROM keywords")
    return {name.lower(): kid for name, kid in cur.fetchall()}


PAPER_SQL = """
INSERT INTO papers (openalex_id, doi, title, abstract, publication_year, publication_date,
                    publication_type, url, cited_by_count, category, topic, journal_id,
                    data_source, retrieved_at)
VALUES (%(openalex_id)s, %(doi)s, %(title)s, %(abstract)s, %(publication_year)s,
        %(publication_date)s, %(publication_type)s, %(url)s, %(cited_by_count)s, %(category)s,
        %(topic)s, %(journal_id)s, %(data_source)s, %(retrieved)s)
ON CONFLICT (doi) DO UPDATE SET
    openalex_id = EXCLUDED.openalex_id, title = EXCLUDED.title, abstract = EXCLUDED.abstract,
    publication_year = EXCLUDED.publication_year, publication_date = EXCLUDED.publication_date,
    publication_type = EXCLUDED.publication_type, url = EXCLUDED.url,
    cited_by_count = EXCLUDED.cited_by_count, category = EXCLUDED.category, topic = EXCLUDED.topic,
    journal_id = EXCLUDED.journal_id, data_source = EXCLUDED.data_source,
    retrieved_at = EXCLUDED.retrieved_at, updated_at = now()
WHERE (papers.openalex_id, papers.title, papers.abstract, papers.publication_year,
       papers.publication_type, papers.url, papers.cited_by_count, papers.category, papers.topic,
       papers.journal_id)
      IS DISTINCT FROM
      (EXCLUDED.openalex_id, EXCLUDED.title, EXCLUDED.abstract, EXCLUDED.publication_year,
       EXCLUDED.publication_type, EXCLUDED.url, EXCLUDED.cited_by_count, EXCLUDED.category,
       EXCLUDED.topic, EXCLUDED.journal_id)
"""


def main():
    url = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("DATABASE_URL")
    if not url:
        sys.exit("Give a PostgreSQL connection URL as the first argument or in $DATABASE_URL.")
    papers = read_papers(DATA_FILE)
    print(f"Read {len(papers)} papers from {DATA_FILE}")

    with psycopg.connect(url) as conn:
        with conn.cursor() as cur:
            with open(SCHEMA_FILE, encoding="utf-8") as f:
                cur.execute(f.read())                    # make sure the tables exist
            journal_ids = upsert_journals(cur, papers)
            author_ids = upsert_authors(cur, papers)
            keyword_ids = upsert_keywords(cur, papers)
            print(f"Journals: {len(journal_ids)}, authors: {len(author_ids)}, keywords: {len(keyword_ids)}")

            for start in range(0, len(papers), BATCH):
                batch = papers[start:start + BATCH]
                rows = []
                for p in batch:
                    j = p.get("journal") or {}
                    rows.append(dict(p, journal_id=journal_ids.get(j.get("openalex_id"))))
                cur.executemany(PAPER_SQL, rows)

                dois = [p["doi"] for p in batch]
                cur.execute("SELECT doi, id FROM papers WHERE doi = ANY(%s)", (dois,))
                paper_ids = dict(cur.fetchall())
                ids = list(paper_ids.values())

                # Rebuild the link tables for this batch so they match the dataset exactly.
                cur.execute("DELETE FROM paper_authors WHERE paper_id = ANY(%s)", (ids,))
                cur.execute("DELETE FROM paper_keywords WHERE paper_id = ANY(%s)", (ids,))
                links_a, links_k = [], []
                for p in batch:
                    pid = paper_ids[p["doi"]]
                    seen = set()
                    for pos, a in enumerate(p["authors"], start=1):
                        aid = author_ids.get(a.get("openalex_id"))
                        if aid and aid not in seen:
                            seen.add(aid)
                            links_a.append((pid, aid, pos))
                    for k in {k.lower() for k in p["keywords"]}:
                        links_k.append((pid, keyword_ids[k]))
                cur.executemany("INSERT INTO paper_authors (paper_id, author_id, position) VALUES (%s, %s, %s)", links_a)
                cur.executemany("INSERT INTO paper_keywords (paper_id, keyword_id) VALUES (%s, %s)", links_k)
                conn.commit()
                print(f"  {min(start + BATCH, len(papers))}/{len(papers)} papers")

            cur.execute("SELECT count(*) FROM papers")
            print(f"Done. papers table now holds {cur.fetchone()[0]} rows.")


if __name__ == "__main__":
    main()
