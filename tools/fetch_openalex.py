import argparse
import gzip
import html
import json
import os
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import date

API = "https://api.openalex.org"
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
RAW_DIR = os.path.join(ROOT, "data", "raw")
OUT_FILE = os.path.join(ROOT, "data", "openalex_papers.jsonl.gz")

SUBFIELDS = {
    1702: "Artificial Intelligence",
    1707: "Computer Vision and Pattern Recognition",
    1705: "Computer Networks and Communications",
    1710: "Information Systems",
    1712: "Software",
    1709: "Human-Computer Interaction",
    1711: "Signal Processing",
    1708: "Hardware and Architecture",
    1703: "Computational Theory and Mathematics",
    1706: "Computer Science Applications",
}
WORK_FIELDS = ("id,doi,title,publication_year,publication_date,type,authorships,"
               "primary_location,keywords,primary_topic,abstract_inverted_index,cited_by_count")
PAGE_SIZE = 200


def api_url(endpoint, params):
    key = os.environ.get("OPENALEX_API_KEY")
    if key:
        params = dict(params, api_key=key)
    return f"{API}/{endpoint}?{urllib.parse.urlencode(params)}"


def get_json(url, tries=6):
    delay = 2.0
    for attempt in range(tries):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "ResearchNet-PBL-importer"})
            with urllib.request.urlopen(req, timeout=60) as resp:
                return json.load(resp)
        except urllib.error.HTTPError as e:
            if e.code not in (429, 500, 502, 503, 504) or attempt == tries - 1:
                raise
            print(f"  HTTP {e.code}, retrying in {delay:.0f}s", file=sys.stderr)
        except (urllib.error.URLError, TimeoutError) as e:
            if attempt == tries - 1:
                raise
            print(f"  network error ({e}), retrying in {delay:.0f}s", file=sys.stderr)
        time.sleep(delay)
        delay *= 2
    raise RuntimeError("unreachable")


def fetch_subfield(subfield_id, per_subfield):
    cache = os.path.join(RAW_DIR, f"works_{subfield_id}_{per_subfield}.jsonl.gz")
    if os.path.exists(cache):
        with gzip.open(cache, "rt", encoding="utf-8") as f:
            return [json.loads(line) for line in f]

    flt = (f"primary_topic.subfield.id:{subfield_id},type:article|review,has_abstract:true,"
           f"has_doi:true,language:en,publication_year:2010-2025")
    works, page = [], 1
    while len(works) < per_subfield:
        params = {"filter": flt, "sort": "cited_by_count:desc", "per-page": PAGE_SIZE,
                  "page": page, "select": WORK_FIELDS}
        data = get_json(api_url("works", params))
        if not data["results"]:
            break
        works.extend(data["results"])
        page += 1
        print(f"  subfield {subfield_id}: {len(works)} works", file=sys.stderr)
        time.sleep(0.3)
    works = works[:per_subfield]

    tmp = cache + ".tmp"
    with gzip.open(tmp, "wt", encoding="utf-8") as f:
        for w in works:
            f.write(json.dumps(w) + "\n")
    os.replace(tmp, cache)
    return works


def fetch_sources(source_ids):
    cache = os.path.join(RAW_DIR, "sources.json")
    known = {}
    if os.path.exists(cache):
        with open(cache, encoding="utf-8") as f:
            known = json.load(f)
    missing = sorted(set(source_ids) - set(known))
    for i in range(0, len(missing), 50):
        batch = missing[i:i + 50]
        params = {"filter": "openalex:" + "|".join(batch), "per-page": 50,
                  "select": "id,display_name,issn_l,type,host_organization_name,summary_stats"}
        data = get_json(api_url("sources", params))
        for s in data["results"]:
            known[short_id(s["id"])] = s
        for sid in batch:
            known.setdefault(sid, None)
        print(f"  sources: {min(i + 50, len(missing))}/{len(missing)}", file=sys.stderr)
        time.sleep(0.3)
        with open(cache, "w", encoding="utf-8") as f:
            json.dump(known, f)
    return known


def short_id(url):
    return url.rsplit("/", 1)[-1] if url else None


TAG_RE = re.compile(r"<[^>]+>")
SPACE_RE = re.compile(r"\s+")
ABSTRACT_LABEL_RE = re.compile(r"^(abstract|summary)\b[\s.:—-]*", re.IGNORECASE)


def clean_text(text):
    if not text:
        return None
    text = html.unescape(TAG_RE.sub("", text))
    text = SPACE_RE.sub(" ", text).strip()
    return text or None


def rebuild_abstract(inverted):
    if not inverted:
        return None
    slots = {}
    for word, positions in inverted.items():
        for p in positions:
            slots[p] = word
    text = clean_text(" ".join(slots[i] for i in sorted(slots)))
    return ABSTRACT_LABEL_RE.sub("", text) if text else None


def clean_work(w, category, sources, retrieved):
    title = clean_text(w.get("title"))
    abstract = rebuild_abstract(w.get("abstract_inverted_index"))
    doi = (w.get("doi") or "").lower().replace("https://doi.org/", "") or None
    if not title or not doi or not abstract or len(abstract.split()) < 30:
        return None

    loc = w.get("primary_location") or {}
    src = loc.get("source") or {}
    src_id = short_id(src.get("id"))
    src_info = sources.get(src_id) if src_id else None
    journal = None
    if src_id:
        impact = None
        stats = (src_info or {}).get("summary_stats") or {}
        if src.get("type") != "repository" and stats.get("2yr_mean_citedness") is not None:
            impact = round(float(stats["2yr_mean_citedness"]), 3)
        journal = {
            "openalex_id": src_id,
            "name": clean_text(src.get("display_name")),
            "issn_l": src.get("issn_l"),
            "type": src.get("type"),
            "publisher": (src_info or {}).get("host_organization_name"),
            "impact_factor": impact,
            "impact_factor_source": "OpenAlex 2yr_mean_citedness" if impact is not None else None,
            "impact_factor_year": retrieved.year if impact is not None else None,
        }

    authors = []
    for a in w.get("authorships") or []:
        info = a.get("author") or {}
        name = clean_text(info.get("display_name"))
        if name:
            authors.append({"openalex_id": short_id(info.get("id")), "name": name})

    keywords = []
    for k in w.get("keywords") or []:
        name = clean_text(k.get("display_name"))
        if name and name.lower() not in (x.lower() for x in keywords):
            keywords.append(name)

    topic = w.get("primary_topic") or {}
    return {
        "openalex_id": short_id(w["id"]),
        "doi": doi,
        "title": title,
        "abstract": abstract,
        "publication_year": w.get("publication_year"),
        "publication_date": w.get("publication_date"),
        "publication_type": w.get("type"),
        "url": loc.get("landing_page_url") or f"https://doi.org/{doi}",
        "cited_by_count": w.get("cited_by_count") or 0,
        "category": category,
        "topic": topic.get("display_name"),
        "keywords": keywords,
        "authors": authors[:50],
        "journal": journal,
        "data_source": "OpenAlex",
        "retrieved": retrieved.isoformat(),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--per", type=int, default=1000, help="papers per subfield (default 1000)")
    args = parser.parse_args()
    os.makedirs(RAW_DIR, exist_ok=True)

    raw = []
    for sid, name in SUBFIELDS.items():
        print(f"Fetching {name} ({sid})", file=sys.stderr)
        raw.extend((w, name) for w in fetch_subfield(sid, args.per))

    source_ids = {short_id(((w.get("primary_location") or {}).get("source") or {}).get("id"))
                  for w, _ in raw}
    sources = fetch_sources([s for s in source_ids if s])

    retrieved = date.today()
    seen_dois, seen_ids, papers, skipped = set(), set(), [], 0
    for w, category in raw:
        p = clean_work(w, category, sources, retrieved)
        if p is None or p["doi"] in seen_dois or p["openalex_id"] in seen_ids:
            skipped += 1
            continue
        seen_dois.add(p["doi"])
        seen_ids.add(p["openalex_id"])
        papers.append(p)

    with gzip.open(OUT_FILE, "wt", encoding="utf-8") as f:
        for p in papers:
            f.write(json.dumps(p, ensure_ascii=False) + "\n")
    print(f"Wrote {len(papers)} papers to {OUT_FILE} (skipped {skipped} incomplete/duplicate)",
          file=sys.stderr)


if __name__ == "__main__":
    main()
