#!/usr/bin/env python3
"""Build frequency-results.csv from results_partial.jsonl + documented access probes,
then verify every Sourcegraph number in the CSV against the saved raw responses."""
import csv, json, os, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
RAW = os.path.join(HERE, "raw")

SEMANTICS = {
    "files": "distinct files with >=1 match (Sourcegraph select:file matchCount)",
    "lines": "matching lines (Sourcegraph content-search matchCount)",
}

def mtime_utc(fn):
    p = os.path.join(RAW, fn)
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(os.path.getmtime(p))) if os.path.exists(p) else ""

rows = []
for line in open(os.path.join(HERE, "results_partial.jsonl")):
    d = json.loads(line)
    is_stream = "stream" in d["index"]
    lb = ""
    notes = ""
    if is_stream:
        lb = "yes" if (d.get("skipped") and "shard-match-limit" in d["skipped"]) else "no"
        notes = ("stream final progress event; done=%s; skipped=%s"
                 % (d.get("done"), ",".join(d.get("skipped") or [])))
        if lb == "yes":
            notes += "; count is a lower bound (shard match limit reached)"
    else:
        lb = "yes" if d.get("limit_hit") else "no"
        notes = ("GraphQL search.results; limitHit=%s; approximateResultCount=%s; server elapsed %s ms"
                 % (d.get("limit_hit"), d.get("approximate"), d.get("elapsed_ms")))
        if d.get("match_count") == 100000 and d.get("limit_hit"):
            notes += "; count equals the count:100000 query ceiling -> true total is higher"
        if d.get("error"):
            notes += "; ERROR: " + d["error"]
    rows.append({
        "index": d["index"], "endpoint": d["endpoint"], "query": d["query"],
        "term": d["term"], "pattern_type": d["pattern_type"], "scope": d["scope"],
        "unit": d["unit"],
        "count": "" if d.get("match_count") is None else d["match_count"],
        "count_semantics": SEMANTICS.get(d["unit"], d["unit"]),
        "count_is_lower_bound": lb,
        "repositories_count": "" if d.get("repositories_count") is None else d["repositories_count"],
        "retrieved_utc": d.get("retrieved_utc", ""), "raw_file": d.get("raw_file", ""),
        "notes": notes,
    })

# ---- documented probes of other indexes / endpoints (no usable global counts) ----
probes = [
    dict(index="Sourcegraph stream API (unauthenticated)", endpoint="GET https://sourcegraph.com/.api/search/stream",
         query="context:global dropWhileEnd select:file (default count)", term="dropWhileEnd",
         pattern_type="literal", scope="all languages", unit="files", count=567,
         count_semantics=SEMANTICS["files"], count_is_lower_bound="yes", repositories_count=255,
         retrieved_utc=mtime_utc("probe_sourcegraph_dropWhileEnd.txt"),
         raw_file="probe_sourcegraph_dropWhileEnd.txt",
         notes="initial probe at the API default result count; final progress reported shard-match-limit; superseded by the count:100000 runs but kept to document count-parameter sensitivity"),
    dict(index="grep.app API", endpoint="GET https://grep.app/api/search?q=dropWhileEnd",
         query="q=dropWhileEnd", term="dropWhileEnd", pattern_type="literal", scope="all languages",
         unit="n/a", count="", count_semantics="no count obtainable", count_is_lower_bound="",
         repositories_count="", retrieved_utc=mtime_utc("probe_grepapp_dropWhileEnd_checkpoint.html"),
         raw_file="probe_grepapp_dropWhileEnd_checkpoint.html",
         notes="BLOCKED: HTTP 429 with a Vercel Security Checkpoint page (bot check requires JavaScript); identical result on retest (raw/retest_grepapp.html). No count could be retrieved."),
    dict(index="GitHub code search API", endpoint="GET https://api.github.com/search/code?q=dropWhileEnd",
         query="q=dropWhileEnd", term="dropWhileEnd", pattern_type="literal", scope="all languages",
         unit="n/a", count="", count_semantics="no count obtainable", count_is_lower_bound="",
         repositories_count="", retrieved_utc=mtime_utc("probe_github_dropWhileEnd.json"),
         raw_file="probe_github_dropWhileEnd.json",
         notes="UNAVAILABLE without credentials: HTTP 401 {\"message\": \"Requires authentication\"}. No authenticated GitHub access was available to this study."),
    dict(index="GitHub web code search (logged out)", endpoint="GET https://github.com/search?q=dropWhileEnd&type=code",
         query="dropWhileEnd type=code", term="dropWhileEnd", pattern_type="literal", scope="all languages",
         unit="n/a", count="", count_semantics="no count obtainable", count_is_lower_bound="",
         repositories_count="", retrieved_utc=mtime_utc("probe_github_html_search.html"),
         raw_file="probe_github_html_search.html",
         notes="UNAVAILABLE logged out: the returned page contains 'Sign in to search code'; GitHub code search results/counts require sign-in."),
    dict(index="searchcode classic API", endpoint="GET https://searchcode.com/api/codesearch_I/?q=dropWhileEnd",
         query="q=dropWhileEnd", term="dropWhileEnd", pattern_type="literal", scope="all languages",
         unit="n/a", count="", count_semantics="no count obtainable", count_is_lower_bound="",
         repositories_count="", retrieved_utc=mtime_utc("probe_searchcode_dropWhileEnd.json"),
         raw_file="probe_searchcode_dropWhileEnd.json",
         notes="RETIRED: HTTP 404 '404 page not found' for /api/codesearch_I/ and for /api/ itself; the classic global code-search API no longer exists."),
    dict(index="searchcode current API (api.searchcode.com)", endpoint="POST https://api.searchcode.com/api/v1/code_search",
         query='{"query":"dropWhileEnd"}', term="dropWhileEnd", pattern_type="literal", scope="n/a",
         unit="n/a", count="", count_semantics="no count obtainable", count_is_lower_bound="",
         repositories_count="", retrieved_utc="2026-10-15",
         raw_file="",
         notes="UNREACHABLE + wrong shape: two POST attempts produced no response (first ran ~600s until the harness timeout; bounded retry timed out at 25s, curl rc=28). Per searchcode.com's own current docs the replacement API is per-repository analysis (a 'repository' parameter), not a global code index, so it cannot produce corpus-wide frequency counts in any case."),
    dict(index="Debian Code Search (codesearch.debian.net)", endpoint="GET https://codesearch.debian.net/search?q=find_last_not_of",
         query="q=find_last_not_of", term="find_last_not_of", pattern_type="regexp (site default)",
         scope="Debian source packages corpus", unit="result pages (qualitative only)", count=945,
         count_semantics="number of result pages offered by the web UI for the query in the Debian source corpus; NOT a file or package count",
         count_is_lower_bound="", repositories_count="",
         retrieved_utc=mtime_utc("probe_debian_find_last_not_of_results.html"),
         raw_file="probe_debian_find_last_not_of_results.html",
         notes="QUALITATIVE corroboration only: the UI paginates results (observed 'Pages: 1 ... 945') and lists many unrelated source packages; the static HTML exposes no trustworthy total file/package count, so none is reported."),
    dict(index="Debian Code Search (codesearch.debian.net)", endpoint="GET https://codesearch.debian.net/search?q=trim_end_matches",
         query="q=trim_end_matches", term="trim_end_matches", pattern_type="regexp (site default)",
         scope="Debian source packages corpus", unit="result pages (qualitative only)", count=150,
         count_semantics="number of result pages offered by the web UI for the query in the Debian source corpus; NOT a file or package count",
         count_is_lower_bound="", repositories_count="",
         retrieved_utc=mtime_utc("probe_debian_trim_end_matches_results.html"),
         raw_file="probe_debian_trim_end_matches_results.html",
         notes="QUALITATIVE corroboration only: 150 result pages; the package filter list is dominated by independently packaged Rust crates (rust-zerocopy, rust-juniper-codegen, rust-indicatif, ...), corroborating broad real-world use of Rust trim_end_matches outside the Sourcegraph corpus."),
]
rows.extend(probes)

FIELDS = ["index", "endpoint", "query", "term", "pattern_type", "scope", "unit", "count",
          "count_semantics", "count_is_lower_bound", "repositories_count", "retrieved_utc",
          "raw_file", "notes"]
out = os.path.join(HERE, "frequency-results.csv")
with open(out, "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=FIELDS)
    w.writeheader()
    for r in rows:
        w.writerow(r)
print("wrote", out, "rows:", len(rows))

# ---------------------------------------------------------------- verification
errors = []
checked = 0
for r in rows:
    fn, idx = r["raw_file"], r["index"]
    if not fn or r["count"] == "":
        continue
    p = os.path.join(RAW, fn)
    if not os.path.exists(p):
        errors.append(f"{fn}: raw file missing")
        continue
    if "GraphQL" in idx:
        d = json.load(open(p))
        res = d["data"]["search"]["results"]
        checked += 1
        if res["matchCount"] != int(r["count"]):
            errors.append(f"{fn}: CSV {r['count']} != raw matchCount {res['matchCount']}")
        if ("yes" if res["limitHit"] else "no") != r["count_is_lower_bound"]:
            errors.append(f"{fn}: lower-bound flag mismatch")
    elif "stream" in idx:
        last = None
        for line in open(p, encoding="utf-8", errors="ignore"):
            if line.startswith("data: "):
                try:
                    dd = json.loads(line[6:])
                except Exception:
                    continue
                if isinstance(dd, dict) and "matchCount" in dd:
                    last = dd
        checked += 1
        if last is None:
            errors.append(f"{fn}: no progress event in raw stream")
        else:
            if last["matchCount"] != int(r["count"]):
                errors.append(f"{fn}: CSV {r['count']} != raw stream matchCount {last['matchCount']}")
            if str(last.get("repositoriesCount")) != str(r["repositories_count"]):
                errors.append(f"{fn}: repos CSV {r['repositories_count']} != raw {last.get('repositoriesCount')}")
print("verified rows against raw:", checked)
if errors:
    print("MISMATCHES:")
    [print(" -", e) for e in errors]
    sys.exit(1)
print("ALL CSV NUMBERS MATCH RAW EVIDENCE")
