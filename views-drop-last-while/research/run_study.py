#!/usr/bin/env python3
"""Reproducible frequency-study runner.
Indexes: Sourcegraph GraphQL + stream (primary), grep.app / GitHub API (access probes).
Writes raw responses into ./raw/ and parsed rows into ./results_partial.jsonl
Run from the research/ directory.
"""
import json, os, re, subprocess, sys, time, urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
RAW = os.path.join(HERE, "raw")
os.makedirs(RAW, exist_ok=True)
OUT = os.path.join(HERE, "results_partial.jsonl")
UA = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36"
GQL = ("query($q: String!) { search(query: $q, version: V2) { results "
       "{ matchCount approximateResultCount limitHit elapsedMilliseconds } } }")

def utcnow():
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())

def curl(args, timeout=420):
    return subprocess.run(["curl", "-s", "-A", UA] + args,
                          capture_output=True, text=True, timeout=timeout)

def log(row):
    with open(OUT, "a") as f:
        f.write(json.dumps(row) + "\n")
    print(json.dumps(row)[:220], flush=True)

# ---------------------------------------------------------------- GraphQL runs
# (slug, query, unit, term, pattern_type, scope)
GQL_QUERIES = [
    # exact direct APIs - files
    ("dropWhileEnd_files",        "context:global dropWhileEnd select:file count:100000", "files", "dropWhileEnd", "literal", "all languages"),
    ("takeWhileEnd_files",        "context:global takeWhileEnd select:file count:100000", "files", "takeWhileEnd", "literal", "all languages"),
    ("dropLastWhile_files",       "context:global dropLastWhile select:file count:100000", "files", "dropLastWhile", "literal", "all languages"),
    ("takeLastWhile_files",       "context:global takeLastWhile select:file count:100000", "files", "takeLastWhile", "literal", "all languages"),
    ("dropRightWhile_files",      "context:global dropRightWhile select:file count:100000", "files", "dropRightWhile", "literal", "all languages"),
    ("takeRightWhile_files",      "context:global takeRightWhile select:file count:100000", "files", "takeRightWhile", "literal", "all languages"),
    # exact direct APIs - matching lines (default content search)
    ("dropWhileEnd_lines",        "context:global dropWhileEnd count:100000", "lines", "dropWhileEnd", "literal", "all languages"),
    ("takeWhileEnd_lines",        "context:global takeWhileEnd count:100000", "lines", "takeWhileEnd", "literal", "all languages"),
    ("dropLastWhile_lines",       "context:global dropLastWhile count:100000", "lines", "dropLastWhile", "literal", "all languages"),
    ("takeLastWhile_lines",       "context:global takeLastWhile count:100000", "lines", "takeLastWhile", "literal", "all languages"),
    ("dropRightWhile_lines",      "context:global dropRightWhile count:100000", "lines", "dropRightWhile", "literal", "all languages"),
    ("takeRightWhile_lines",      "context:global takeRightWhile count:100000", "lines", "takeRightWhile", "literal", "all languages"),
    # string analogues - files + lines
    ("find_last_not_of_files",    "context:global find_last_not_of select:file count:100000", "files", "find_last_not_of", "literal", "all languages"),
    ("find_last_not_of_lines",    "context:global find_last_not_of count:100000", "lines", "find_last_not_of", "literal", "all languages"),
    ("trim_end_matches_files",    "context:global trim_end_matches select:file count:100000", "files", "trim_end_matches", "literal", "all languages"),
    ("trim_end_matches_lines",    "context:global trim_end_matches count:100000", "lines", "trim_end_matches", "literal", "all languages"),
    ("trim_end_files",            "context:global trim_end select:file count:100000", "files", "trim_end", "literal", "all languages"),
    ("trim_right_if_files",       "context:global trim_right_if select:file count:100000", "files", "trim_right_if", "literal", "all languages"),
    ("rstrip_files",              "context:global rstrip select:file count:100000", "files", "rstrip", "literal", "all languages"),
    ("rstrip_lines",              "context:global rstrip count:100000", "lines", "rstrip", "literal", "all languages"),
    # language-scoped - files
    ("dropLastWhile_kotlin_files",   "context:global dropLastWhile lang:kotlin select:file count:100000", "files", "dropLastWhile", "literal", "Kotlin"),
    ("takeLastWhile_kotlin_files",   "context:global takeLastWhile lang:kotlin select:file count:100000", "files", "takeLastWhile", "literal", "Kotlin"),
    ("dropLastWhile_ts_files",       "context:global dropLastWhile lang:typescript select:file count:100000", "files", "dropLastWhile", "literal", "TypeScript"),
    ("dropLastWhile_js_files",       "context:global dropLastWhile lang:javascript select:file count:100000", "files", "dropLastWhile", "literal", "JavaScript"),
    ("takeLastWhile_ts_files",       "context:global takeLastWhile lang:typescript select:file count:100000", "files", "takeLastWhile", "literal", "TypeScript"),
    ("dropWhileEnd_haskell_files",   "context:global dropWhileEnd lang:haskell select:file count:100000", "files", "dropWhileEnd", "literal", "Haskell"),
    ("trim_end_matches_rust_files",  "context:global trim_end_matches lang:rust select:file count:100000", "files", "trim_end_matches", "literal", "Rust"),
    ("trim_end_rust_files",          "context:global trim_end lang:rust select:file count:100000", "files", "trim_end", "literal", "Rust"),
    ("rstrip_python_files",          "context:global rstrip lang:python select:file count:100000", "files", "rstrip", "literal", "Python"),
    ("find_last_not_of_cpp_files",   "context:global find_last_not_of lang:c++ select:file count:100000", "files", "find_last_not_of", "literal", "C++"),
    ("trim_right_if_cpp_files",      "context:global trim_right_if lang:c++ select:file count:100000", "files", "trim_right_if", "literal", "C++"),
    ("dropWhileEnd_cpp_files",       "context:global dropWhileEnd lang:c++ select:file count:100000", "files", "dropWhileEnd", "literal", "C++"),
    # C++ manual patterns - regexp, files
    ("cpp_findif_rbegin_files",      r"context:global lang:c++ patterntype:regexp find_if\(.*rbegin select:file count:100000", "files", "find_if(...rbegin", "regexp", "C++"),
    ("cpp_findif_crbegin_files",     r"context:global lang:c++ patterntype:regexp find_if\(.*crbegin select:file count:100000", "files", "find_if(...crbegin", "regexp", "C++"),
    ("cpp_while_back_files",         r"context:global lang:c++ patterntype:regexp while\s*\(.*\.back\(\) select:file count:100000", "files", "while(...back()", "regexp", "C++"),
    ("cpp_rbegin_files",             r"context:global lang:c++ patterntype:regexp \brbegin\(\) select:file count:100000", "files", "rbegin()", "regexp", "C++"),
    ("cpp_findif_rbegin_lines",      r"context:global lang:c++ patterntype:regexp find_if\(.*rbegin count:100000", "lines", "find_if(...rbegin", "regexp", "C++"),
    ("cpp_while_back_lines",         r"context:global lang:c++ patterntype:regexp while\s*\(.*\.back\(\) count:100000", "lines", "while(...back()", "regexp", "C++"),
]

def run_gql(slug, query, unit, term, ptype, scope):
    payload = {"query": GQL, "variables": {"q": query}}
    fn = os.path.join(RAW, f"sourcegraph_gql__{slug}.json")
    row = {"index": "Sourcegraph GraphQL API (unauthenticated)",
           "endpoint": "POST https://sourcegraph.com/.api/graphql",
           "query": query, "term": term, "pattern_type": ptype, "scope": scope,
           "unit": unit, "retrieved_utc": utcnow(), "raw_file": os.path.basename(fn)}
    for attempt in (1, 2):
        try:
            r = curl(["-X", "POST", "https://sourcegraph.com/.api/graphql",
                      "-H", "Content-Type: application/json", "--data", json.dumps(payload),
                      "-w", "\nHTTPSTATUS:%{http_code}"], timeout=420)
            body, _, status = r.stdout.rpartition("\nHTTPSTATUS:")
            with open(fn, "w") as f:
                f.write(body)
            row["http"] = status.strip()
            d = json.loads(body)
            res = d["data"]["search"]["results"]
            row.update({"match_count": res["matchCount"],
                        "approximate": res["approximateResultCount"],
                        "limit_hit": res["limitHit"],
                        "elapsed_ms": res["elapsedMilliseconds"]})
            break
        except Exception as e:
            row["error"] = f"attempt {attempt}: {type(e).__name__}: {e}"
            if attempt == 1:
                time.sleep(45)
    log(row)
    time.sleep(4)

# ----------------------------------------------------------------- stream runs
STREAM_TERMS = ["dropWhileEnd", "takeWhileEnd", "dropLastWhile", "takeLastWhile",
                "dropRightWhile", "takeRightWhile", "find_last_not_of"]

def run_stream(term):
    q = f"context:global {term} select:file count:100000"
    url = ("https://sourcegraph.com/.api/search/stream?q=" +
           urllib.parse.quote(q) + "&v=3&t=literal")
    fn = os.path.join(RAW, f"sourcegraph_stream__{term}_files.sse.txt")
    row = {"index": "Sourcegraph stream API (unauthenticated)",
           "endpoint": "GET https://sourcegraph.com/.api/search/stream",
           "query": q, "term": term, "pattern_type": "literal", "scope": "all languages",
           "unit": "files", "retrieved_utc": utcnow(), "raw_file": os.path.basename(fn)}
    try:
        r = curl([url], timeout=600)
        with open(fn, "w") as f:
            f.write(r.stdout)
        last = None
        for line in r.stdout.splitlines():
            if line.startswith("data: "):
                try:
                    d = json.loads(line[6:])
                except Exception:
                    continue
                if isinstance(d, dict) and "matchCount" in d:
                    last = d
        if last:
            row.update({"match_count": last.get("matchCount"),
                        "repositories_count": last.get("repositoriesCount"),
                        "done": last.get("done"),
                        "skipped": [s.get("reason") for s in last.get("skipped", [])]})
        else:
            row["error"] = "no progress event parsed"
    except Exception as e:
        row["error"] = f"{type(e).__name__}: {e}"
    log(row)
    time.sleep(4)

def main():
    which = sys.argv[1] if len(sys.argv) > 1 else "all"
    if which in ("all", "gql"):
        for spec in GQL_QUERIES:
            run_gql(*spec)
    if which in ("all", "stream"):
        for t in STREAM_TERMS:
            run_stream(t)
    print("DONE", which, utcnow(), flush=True)

if __name__ == "__main__":
    main()
