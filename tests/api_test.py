import http.cookiejar
import json
import os
import statistics
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

BASE = (sys.argv[1] if len(sys.argv) > 1 else "http://localhost:8080").rstrip("/")
DB_URL = sys.argv[2] if len(sys.argv) > 2 else os.environ.get("DATABASE_URL")
RUN = str(int(time.time()))
results = []


class Client:

    def __init__(self):
        self.opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))

    def call(self, method, path, body=None, params=None, headers=None):
        url = BASE + path + ("?" + urllib.parse.urlencode(params) if params else "")
        data = json.dumps(body).encode() if body is not None else None
        req = urllib.request.Request(url, data=data, method=method)
        if body is not None:
            req.add_header("Content-Type", "application/json")
        for k, v in (headers or {}).items():
            req.add_header(k, v)
        try:
            with self.opener.open(req, timeout=30) as resp:
                return resp.status, json.loads(resp.read() or b"null")
        except urllib.error.HTTPError as e:
            raw = e.read()
            try:
                return e.code, json.loads(raw)
            except ValueError:
                return e.code, raw.decode(errors="replace")

    def get(self, path, **params):
        return self.call("GET", path, params=params or None)


def check(name, ok, detail=""):
    results.append((bool(ok), name, detail))
    print(("PASS " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""))


def search(client, **params):
    status, data = client.get("/api/search", **params)
    assert status == 200, (status, data)
    return data


def all_results(client, **params):
    items, page = [], 1
    while True:
        data = search(client, page=page, page_size=50, **params)
        items += data["results"]
        if page * 50 >= data["total"]:
            return items, data["total"]
        page += 1


def main():
    anon = Client()

    status, health = anon.get("/api/health")
    check("health endpoint", status == 200 and health["status"] == "ok" and health["papers"] > 0)
    status, stats = anon.get("/api/stats")
    check("stats: papers and index sizes", status == 200 and stats["indexedTerms"] > 1000 and stats["trieNodes"] > 1000)

    both = search(anon, q="graph neural network")
    either = search(anon, q="graph or neural or network")
    check("AND search returns fewer results than OR", 0 < both["total"] < either["total"],
          f"{both['total']} vs {either['total']}")
    top = both["results"][0]
    _, detail = anon.get(f"/api/papers/{top['id']}")
    text = " ".join([detail["title"], detail["abstract"] or "", " ".join(detail["keywords"]), detail["topic"] or ""]).lower()
    check("top AND result contains every query word", all(w in text for w in ["graph", "neural", "network"]))
    rel = [r["relevance"] for r in both["results"]]
    check("results ranked by relevance (descending)", rel == sorted(rel, reverse=True))
    check("relaxed search when no paper has all words", search(anon, q="quantum zebra")["relaxed"] is True)
    check("stop-word-only query = browse all", search(anon, q="the of and")["total"] == stats["papers"])
    check("search by author name", any("Kaiming He" in ", ".join(r["authors"]) for r in search(anon, q="kaiming he")["results"]))
    check("search by journal name", search(anon, q="IEEE Access")["total"] > 0)

    items, total = all_results(anon, year_from=2019, year_to=2020)
    check("year range filter", total > 0 and all(2019 <= r["year"] <= 2020 for r in items), f"{total} papers")
    items, _ = all_results(anon, year=2015, q="network")
    check("exact year filter", items and all(r["year"] == 2015 for r in items))
    items, total = all_results(anon, min_impact=10, max_impact=20)
    check("impact range filter (and no missing values)",
          total > 0 and all(r["impactFactor"] is not None and 10 <= r["impactFactor"] <= 20 for r in items))
    items, _ = all_results(anon, category="Software")
    check("category filter", items and all(r["category"] == "Software" for r in items))
    items, _ = all_results(anon, type="review")
    check("publication type filter", items and all(r["type"] == "review" for r in items))
    items, _ = all_results(anon, journal="ieee access")
    check("journal filter (case-insensitive)", items and all("ieee access" in r["journal"].lower() for r in items))
    sample = search(anon, author="Niyato", page_size=5)["results"]
    ok = bool(sample)
    for r in sample:
        _, d = anon.get(f"/api/papers/{r['id']}")
        ok = ok and any(a["name"].split()[-1].lower().startswith("niyato") or " niyato" in a["name"].lower() for a in d["authors"])
    check("author filter", ok)
    combined = search(anon, q="deep learning", author="Wang", year_from=2017, year_to=2022, min_impact=3, max_impact=60,
                      category="Artificial Intelligence", sort="impact_desc", page_size=50)
    rs = combined["results"]
    check("combined filters all hold together",
          combined["total"] > 0 and all(2017 <= r["year"] <= 2022 and 3 <= r["impactFactor"] <= 60 and
                                        r["category"] == "Artificial Intelligence" for r in rs), f"{combined['total']} papers")

    def sorted_by(sort, key, reverse):
        items = search(anon, q="security", sort=sort, page_size=50)["results"]
        values = [key(r) for r in items]
        present = [v for v in values if v is not None]
        nulls_last = values[:len(present)] == present
        return items and present == sorted(present, reverse=reverse) and nulls_last

    check("sort newest first", sorted_by("newest", lambda r: r["publicationDate"], True))
    check("sort oldest first", sorted_by("oldest", lambda r: r["publicationDate"], False))
    check("sort impact high to low (unknown last)", sorted_by("impact_desc", lambda r: r["impactFactor"], True))
    check("sort impact low to high (unknown last)", sorted_by("impact_asc", lambda r: r["impactFactor"], False))
    check("sort most cited", sorted_by("citations", lambda r: r["citedBy"], True))
    p1 = search(anon, q="learning", page=1, page_size=10)
    p2 = search(anon, q="learning", page=2, page_size=10)
    check("pagination: pages do not overlap", not {r["id"] for r in p1["results"]} & {r["id"] for r in p2["results"]}
          and p1["total"] == p2["total"])

    for params, name in [({"sort": "bogus"}, "unknown sort"), ({"year_from": 2020, "year_to": 2010}, "reversed year range"),
                         ({"page": "x"}, "non-numeric page"), ({"page_size": 500}, "page size too large"),
                         ({"min_impact": 9, "max_impact": 2}, "reversed impact range")]:
        status, _ = anon.get("/api/search", **params)
        check(f"400 for {name}", status == 400)
    status, data = anon.get("/api/search", author="' OR 1=1 --")
    check("SQL injection text in a filter is treated as plain text", status == 200 and data["total"] == 0)
    status, _ = anon.get("/api/papers/99999999")
    check("unknown paper -> 404", status == 404)
    status, _ = anon.get("/api/does-not-exist")
    check("unknown API route -> JSON 404", status == 404)

    status, sugg = anon.get("/api/autocomplete", prefix="Reinforc", limit=8)
    check("autocomplete: case-insensitive prefix matches",
          status == 200 and sugg and all(s["text"].lower().startswith("reinforc") for s in sugg), str(sugg[:2]))
    texts = [s["text"].lower() for s in sugg]
    check("autocomplete: no duplicates", len(texts) == len(set(texts)))
    title = top["title"]
    status, sugg = anon.get("/api/autocomplete", prefix=title[:30], limit=10)
    check("autocomplete: paper titles open the paper",
          any(s["kind"] == "paper" and s.get("paperId") == top["id"] for s in sugg), title[:30])
    check("autocomplete: limit respected", len(anon.get("/api/autocomplete", prefix="a", limit=3)[1]) == 3)

    pid = top["id"]
    status, similar = anon.get(f"/api/papers/{pid}/similar", limit=8)
    scores = [s["score"] for s in similar]
    check("similar papers: never the paper itself", status == 200 and similar and all(s["paper"]["id"] != pid for s in similar))
    check("similar papers: sorted, scores in 0..1, with reasons",
          scores == sorted(scores, reverse=True) and all(0 <= s <= 1 for s in scores) and all(s["reasons"] for s in similar))

    alice, bob = Client(), Client()
    a_email, b_email = f"alice.{RUN}@example.com", f"bob.{RUN}@example.com"
    status, _ = alice.call("POST", "/api/register", {"name": "Alice <script>x</script>", "email": a_email.upper(), "password": "alice-pass-1"})
    check("register", status == 201)
    status, _ = bob.call("POST", "/api/register", {"name": "Bob", "email": b_email, "password": "bob-pass-12"})
    status, _ = anon.call("POST", "/api/register", {"name": "Dup", "email": a_email, "password": "whatever1"})
    check("duplicate email (any case) -> 409", status == 409)
    for body, name in [({"name": "X", "email": "not-an-email", "password": "abcdefgh"}, "invalid email"),
                       ({"name": "X", "email": f"x{RUN}@ex.com", "password": "short"}, "short password"),
                       ({"name": "", "email": f"y{RUN}@ex.com", "password": "abcdefgh"}, "missing name"),
                       ({"name": "X", "email": f"z{RUN}@ex.com", "password": "abcdefgh", "confirmPassword": "zzzzzzzz"}, "password mismatch")]:
        check(f"register rejects {name}", anon.call("POST", "/api/register", body)[0] == 400)
    status, me = alice.get("/api/me")
    check("session cookie logs the user in; name stored as plain text",
          status == 200 and me["user"]["email"] == a_email and me["user"]["name"] == "Alice <script>x</script>")
    check("wrong password -> 401", Client().call("POST", "/api/login", {"email": a_email, "password": "nope-nope"})[0] == 401)
    check("unknown email -> same 401", Client().call("POST", "/api/login", {"email": f"ghost{RUN}@x.com", "password": "nope-nope"})[0] == 401)
    check("protected route without login -> 401", anon.get("/api/bookmarks")[0] == 401)
    status, _ = alice.call("POST", "/api/bookmarks", {"paperId": pid}, headers={"Origin": "https://evil.example"})
    check("cross-site request blocked -> 403", status == 403)
    form = urllib.request.Request(BASE + "/api/login", data=b"email=a&password=b", method="POST")
    form.add_header("Content-Type", "application/x-www-form-urlencoded")
    try:
        urllib.request.urlopen(form, timeout=30)
        status = 200
    except urllib.error.HTTPError as e:
        status = e.code
    check("HTML-form style post (not JSON) -> 415", status == 415)

    check("bookmark add", alice.call("POST", "/api/bookmarks", {"paperId": pid})[0] == 201)
    check("bookmark add twice is fine", alice.call("POST", "/api/bookmarks", {"paperId": pid})[0] == 201)
    check("bookmark unknown paper -> 404", alice.call("POST", "/api/bookmarks", {"paperId": 99999999})[0] == 404)
    _, marks = alice.get("/api/bookmarks")
    check("bookmark listed once", [m["id"] for m in marks] == [pid])
    check("search shows bookmarked flag", search(alice, q=top["title"][:40])["results"][0]["bookmarked"] in (True, False))
    _, bob_marks = bob.get("/api/bookmarks")
    check("another user cannot see the bookmark", bob_marks == [])
    check("another user cannot delete the bookmark", bob.call("DELETE", f"/api/bookmarks/{pid}")[0] == 404)

    search(alice, q="federated learning", year_from=2020)
    search(alice, q="federated learning", year_from=2020)
    search(alice, q="federated learning", year_from=2020, page=2)
    viewed = search(anon, q="edge computing")["results"][0]["id"]
    alice.get(f"/api/papers/{viewed}")
    _, hist = alice.get("/api/history")
    fl = [s for s in hist["searches"] if s["query"] == "federated learning"]
    check("search recorded once (repeats and page turns ignored)", len(fl) == 1 and fl[0]["filters"] == {"year_from": 2020})
    check("viewed paper recorded", [p["id"] for p in hist["viewed"]] == [viewed])
    _, bob_hist = bob.get("/api/history")
    check("another user's history is empty", bob_hist == {"searches": [], "viewed": []})
    check("another user cannot delete a search", bob.call("DELETE", f"/api/history/searches/{fl[0]['id']}")[0] == 404)

    status, prof = alice.call("PUT", "/api/profile", {"name": "Alice A.", "interests": ["edge computing", "edge computing", "privacy"]})
    check("profile update (duplicates removed)", status == 200 and prof["user"]["name"] == "Alice A." and prof["interests"] == ["edge computing", "privacy"])
    check("too many interests -> 400", alice.call("PUT", "/api/profile", {"interests": [str(i) for i in range(30)]})[0] == 400)
    status, recs = alice.get("/api/recommendations", limit=15)
    rec_ids = [r["paper"]["id"] for r in recs["results"]]
    check("recommendations returned with explanations", status == 200 and rec_ids and all(r["reasons"] for r in recs["results"]))
    check("recommendations skip saved and viewed papers", pid not in rec_ids and viewed not in rec_ids)
    check("recommendation profile counts", recs["basedOn"]["saved"] == 1 and recs["basedOn"]["viewed"] == 1)
    check("change password: wrong current -> 403",
          alice.call("PUT", "/api/profile/password", {"currentPassword": "bad", "newPassword": "new-pass-123"})[0] == 403)
    check("change password", alice.call("PUT", "/api/profile/password", {"currentPassword": "alice-pass-1", "newPassword": "new-pass-123"})[0] == 200)
    check("login with new password", Client().call("POST", "/api/login", {"email": a_email, "password": "new-pass-123"})[0] == 200)
    check("logout", alice.call("POST", "/api/logout")[0] == 200 and alice.get("/api/me")[0] == 401)

    carol = Client()
    c_email = f"carol.{RUN}@example.com"
    carol.call("POST", "/api/register", {"name": "Carol", "email": c_email, "password": "carol-pass-1"})
    carol.call("POST", "/api/bookmarks", {"paperId": pid})
    check("delete account: login required", anon.call("POST", "/api/account/delete", {"password": "x"})[0] == 401)
    check("delete account: wrong password -> 403", carol.call("POST", "/api/account/delete", {"password": "wrong-one"})[0] == 403)
    check("delete account", carol.call("POST", "/api/account/delete", {"password": "carol-pass-1"})[0] == 200)
    check("deleted account is logged out and cannot log in",
          carol.get("/api/me")[0] == 401 and Client().call("POST", "/api/login", {"email": c_email, "password": "carol-pass-1"})[0] == 401)
    check("email can be registered again after deletion",
          Client().call("POST", "/api/register", {"name": "Carol", "email": c_email, "password": "carol-pass-2"})[0] == 201)

    if DB_URL:
        import psycopg
        with psycopg.connect(DB_URL) as conn, conn.cursor() as cur:
            cur.execute("SELECT password_hash FROM users WHERE email = %s", (a_email,))
            h = cur.fetchone()[0]
            check("password stored as bcrypt hash, not plain text", h.startswith("$2a$") and "new-pass-123" not in h)
            cur.execute("""SELECT count(*) FROM papers p JOIN journals j ON j.id = p.journal_id
                            WHERE p.publication_year BETWEEN 2019 AND 2020 AND j.impact_factor BETWEEN 10 AND 20
                              AND p.category = 'Artificial Intelligence'""")
            expected = cur.fetchone()[0]
            got = search(anon, year_from=2019, year_to=2020, min_impact=10, max_impact=20, category="Artificial Intelligence")["total"]
            check("filter count equals independent SQL count", got == expected, f"api {got} vs sql {expected}")
            cur.execute("""SELECT count(DISTINCT p.id) FROM papers p JOIN paper_authors pa ON pa.paper_id = p.id
                             JOIN authors a ON a.id = pa.author_id
                            WHERE a.name ~* '(^| )niyato' AND p.publication_year >= 2018""")
            expected = cur.fetchone()[0]
            got = search(anon, author="niyato", year_from=2018)["total"]
            check("author filter count equals SQL count", got == expected, f"api {got} vs sql {expected}")
            cur.execute("SELECT count(*) FROM papers")
            check("API paper count equals table count", stats["papers"] == cur.fetchone()[0])

    times = []
    for q in ["deep learning", "graph neural network", "wireless sensor", "blockchain", "image segmentation",
              "software testing", "user experience", "quantum", "federated learning", "intrusion detection"]:
        for params in ({"q": q}, {"q": q, "year_from": 2018, "min_impact": 2, "sort": "impact_desc"}):
            start = time.perf_counter()
            search(anon, **params)
            times.append((time.perf_counter() - start) * 1000)
    start = time.perf_counter()
    for prefix in ["ma", "neur", "deep le", "secur", "blo"]:
        anon.get("/api/autocomplete", prefix=prefix)
    auto_ms = (time.perf_counter() - start) * 1000 / 5
    print(f"latency: search median {statistics.median(times):.1f} ms, max {max(times):.1f} ms; autocomplete avg {auto_ms:.1f} ms")
    check("search median latency under 300 ms", statistics.median(times) < 300)

    passed = sum(ok for ok, _, _ in results)
    print(f"\n{passed}/{len(results)} API checks passed")
    for ok, name, detail in results:
        if not ok:
            print(f"  FAILED: {name} {detail}")
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
