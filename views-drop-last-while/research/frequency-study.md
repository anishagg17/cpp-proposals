# How often do `drop_last_while` / `take_last_while` patterns appear in public code?

A reproducible frequency study supporting the motivation section of the proposed
C++ `views::drop_last_while` / `views::take_last_while`, prepared in response to
Hewill Kang's request (std-proposals, 15 Oct 2026) to research *"how frequently
these utilities are used in projects on GitHub or across different programming
languages."*

- **Retrieval window:** 2026-10-15 18:54–20:03 UTC (all counts are a snapshot of that window)
- **Definitions:** *drop* removes the maximal trailing run of elements satisfying a
  predicate; *take* retains only that trailing run.
- **Machine-readable results:** `frequency-results.csv` (one row per query/index,
  with the exact query, endpoint, count semantics, and raw-evidence filename)
- **Raw evidence:** `raw/` (GraphQL JSON responses, stream-API SSE captures,
  access-probe captures). `verification.txt` records the automated check that
  every Sourcegraph number in the CSV matches its raw response (46/46).

## Headline findings

1. **Where a trailing-while operation exists, both directions are used at scale.**
   Each spelling family shows drop and take within ~3× of each other in file
   counts: Haskell `*WhileEnd` 740 : ≥218; Kotlin/Ramda `*LastWhile`
   ≥4,876 : 3,098; lodash `*RightWhile` ≥13,396 : 13,852. The pair — not just one
   direction — is what programmers reach for.
2. **The usage is broad, not concentrated.** Each exact spelling appears in
   95–2,039 distinct repositories on a single index (see Table A).
3. **The string form of the same operation is enormous.** Rust's
   `trim_end_matches` appears in ≥56,425 files; Python's `rstrip` exceeded the
   100,000-file measurement ceiling in both the global and the Python-only
   query. Trailing-run trimming is one of the most common string operations in
   public code.
4. **C++ programmers currently hand-roll the pattern.** The same-line
   `find_if(…rbegin…)` idiom appears in 6,006 C++ files, `while (… .back())`
   loops in ≥15,783 C++ files, and for strings `find_last_not_of` in
   ≥25,071 C++ files (≥29,654 across all languages). The Haskell spelling
   `dropWhileEnd` appears in exactly **0** C++ files — there is no incumbent
   name for this operation in C++.

## Methods

### Indexes attempted

| Index | Access on 2026-10-15 | Used for |
|---|---|---|
| Sourcegraph public search (GraphQL API + stream API), unauthenticated | Works; slow (50–90 s/query) and subject to anonymous result limits | **All quantitative counts** |
| grep.app API | **Blocked** — HTTP 429 with a Vercel Security Checkpoint (JavaScript bot check), twice | — (evidence in `raw/`) |
| GitHub code search API | **Unavailable** — HTTP 401 "Requires authentication"; the web UI likewise demands sign-in for code search | — (evidence in `raw/`) |
| searchcode | Classic API **retired** (HTTP 404); replacement API is per-repository and was unreachable from this host (connection timeouts) | — (documented in CSV) |
| Debian Code Search (codesearch.debian.net) | Works via its polling web UI, but exposes no trustworthy total counts | Qualitative corroboration on an independent corpus only |

Because grep.app, GitHub and searchcode could not be queried without
credentials, a second *quantitative* cross-check was not obtainable. Instead,
every headline Sourcegraph count was produced by **two independent endpoints**
(GraphQL and the streaming API) and the two agree wherever both are
untruncated (e.g. `dropWhileEnd` files: 740 = 740; `takeLastWhile` files:
3,098 = 3,098). Debian Code Search independently corroborates the two terms
checked there (see §Limitations).

### What the Sourcegraph numbers mean

- Every query ran against `context:global` with `count:100000`, forks and
  archived repositories excluded (Sourcegraph defaults).
- **"Files"** = GraphQL `matchCount` with `select:file` — distinct files
  containing at least one match. **"Matching lines"** = `matchCount` of the
  default content search. **"Repos"** = `repositoriesCount` from the stream
  API's final progress event for the same file query.
- When Sourcegraph reported `limitHit` (GraphQL) or `shard-match-limit`
  (stream), the count is a **lower bound** and is written with `≥`. Counts
  without those flags are reported as exact for this corpus and snapshot.
- Literal identifier searches, except: the C++ manual patterns (§Table D) are
  RE2 regular expressions, and `lang:` filters scope the language-specific
  rows. Full query strings are in the CSV.
- Ecosystem attribution of each spelling was verified against project
  documentation on 2026-10-15: Haskell `Data.List` (`dropWhileEnd`, since
  base 4.5.0.0) and the Haskell ecosystem for `takeWhileEnd`; Kotlin stdlib
  and Ramda for `dropLastWhile`/`takeLastWhile`; lodash for
  `dropRightWhile`/`takeRightWhile`; Rust `str` methods for
  `trim_end`/`trim_end_matches`; Python `str.rstrip`; Boost string algorithms
  for `trim_right_if`.
- The runner scripts (`run_study.py`, `run_remaining.py` — the study was
  split across a runtime restart) and the CSV builder/verifier
  (`build_csv.py`) are kept alongside this file; rerunning them reproduces
  the CSV from live queries.

### Confidence labels

- **High** — exact counts (`limitHit` not set), cross-checked across two
  endpoints. Safe to quote as corpus counts.
- **Medium** — lower bounds (a limit flag was set). Safe to quote only as
  "at least" figures; the true value is higher, by an unknown amount.
- **Low / qualitative** — Debian page counts and any figure at the
  100,000 ceiling; used only to establish presence and rough scale.

## Table A — Exact trailing-while APIs, all languages (Sourcegraph)

| Spelling | Ecosystem | Files | Basis | Matching lines | Repos |
|---|---|---:|---|---:|---:|
| `dropWhileEnd` | Haskell | 740 | exact — High | 1,639 (exact) | 308 |
| `takeWhileEnd` | Haskell | ≥218 | lower bound — Medium | 478 (exact) | 95 |
| `dropLastWhile` | Kotlin stdlib, Ramda | ≥4,876 | lower bound — Medium | 9,076 (exact) | 1,742 |
| `takeLastWhile` | Kotlin stdlib, Ramda | 3,098 | exact — High | 5,559 (exact) | 894 |
| `dropRightWhile` | lodash | ≥13,396 | lower bound — Medium | 44,153 (exact) | 2,039 |
| `takeRightWhile` | lodash | 13,852 | exact — High | 51,229 (exact) | 1,992 |

What the matching files actually are (language tally of the file sets
returned by the stream API):

- `dropWhileEnd`: 84% Haskell (618/740); repositories include `ghc/ghc`,
  `jgm/pandoc`, `hledgerorg/hledger`, `commercialhaskell/stack`,
  `digital-asset/daml` — mainstream production Haskell, not just the
  standard library's own source.
- `takeWhileEnd`: 87% Haskell (189/218).
- `dropLastWhile`: JavaScript 2,191 and Kotlin 2,178 of 4,876 returned
  files — a near-even split between Ramda-style JavaScript and the Kotlin
  standard library. `takeLastWhile`: JavaScript 2,122, Kotlin 454.
- `dropRightWhile` / `takeRightWhile`: ~90% JavaScript (lodash), in
  ≈2,000 repositories each.

## Table B — String analogues of trailing-run trimming (Sourcegraph)

| Spelling | Meaning | Files | Matching lines | Repos |
|---|---|---:|---:|---:|
| `find_last_not_of` | `std::string` search used to locate the end of the trailing run to trim | ≥29,654 | ≥96,549 | 6,977 |
| `trim_end_matches` | Rust `str`: strip the trailing run matching a pattern | ≥56,425 | ≥90,284 | — |
| `trim_end` | Rust `str`: strip trailing whitespace/pattern | ≥80,146 | not run | — |
| `trim_right_if` | Boost string algorithm: trim trailing elements satisfying a predicate | ≥614 | not run | — |
| `rstrip` | Python `str`: strip trailing characters | ≥100,000 (ceiling) | ≥100,000 (ceiling) | — |

Notes: for `trim_end_matches`, the Rust-only query is itself exhaustive at
56,425 files, so the global figure is *at least* that; the global query's own
truncated return (50,024) is lower. For `find_last_not_of`, 88% of the files
in the stream return are C++ (16,943/19,231), and the C++-scoped floor is
≥25,071 files. `trim_end` is a noisy identifier outside Rust; its Rust-scoped
count is the meaningful one (Table C).

## Table C — Language-scoped confirmations (files, Sourcegraph GraphQL)

| Query | Files | Basis |
|---|---:|---|
| `dropLastWhile` in Kotlin | 2,192 | exact — High |
| `takeLastWhile` in Kotlin | 456 | exact — High |
| `dropLastWhile` in JavaScript | 2,195 | exact — High |
| `dropLastWhile` in TypeScript | 133 | exact — High |
| `takeLastWhile` in TypeScript | 138 | exact — High |
| `dropWhileEnd` in Haskell | 618 | exact — High (84% of its global total) |
| `dropWhileEnd` in C++ | 0 | exact — High |
| `trim_end_matches` in Rust | 56,425 | exact — High |
| `trim_end` in Rust | 79,581 | exact — High |
| `rstrip` in Python | ≥100,000 | ceiling — Low |
| `find_last_not_of` in C++ | ≥25,071 | lower bound — Medium |
| `trim_right_if` in C++ | 483 | exact — High |

## Table D — The pattern hand-rolled in C++ today (regexp, `lang:c++`)

| Pattern (same-line, RE2) | Files | Matching lines | Basis |
|---|---:|---:|---|
| `find_if\(.*rbegin` — search from the reverse begin to find where the trailing run starts | 6,006 | 7,145 | exact — High |
| `find_if\(.*crbegin` — const variant | 229 | not run | exact — High |
| `while\s*\(.*\.back\(\)` — loop testing the last element | ≥15,783 | ≥16,988 | lower bound — Medium |
| `\brbegin\(\)` anywhere (context only) | ≥66,351 | not run | lower bound — Medium |

## Limitations

1. **One quantitative index.** All counts come from Sourcegraph's public
   corpus (which excludes forks/archived repos by default and does not cover
   every public repository). grep.app was bot-blocked, GitHub code search
   requires authentication this study did not have, and searchcode's global
   API no longer exists — each failure is evidenced in `raw/` and the CSV.
   Cross-checks were therefore done across two Sourcegraph endpoints, plus
   qualitative corroboration on Debian's independent corpus:
   `find_last_not_of` returns 945 result pages there across many unrelated
   source packages, and `trim_end_matches` returns 150 result pages whose
   package list is dominated by independently packaged Rust crates.
2. **Anonymous-tier truncation.** Several queries hit Sourcegraph's internal
   limits; those numbers are floors (marked `≥`). Runs are also slightly
   nondeterministic: the identical `dropWhileEnd` file query returned 740 on
   both runs but with different `limitHit` flags, and `dropLastWhile` files
   returned 4,738 (GraphQL) vs 4,876 (stream). Quote figures as a
   2026-10-15 snapshot, not a census.
3. **`rstrip` is beyond the measurable range** of the anonymous tier: both
   the global and Python-only queries saturated the 100,000 count ceiling.
   Only "at least 100,000 files" can honestly be claimed.
4. **Identifier counts measure footprint, not pure call sites.** Definitions,
   re-implementations, tests, docs, and vendored/copied code are included.
   This inflates file counts most for JavaScript (a few repositories vendor
   library copies — two repositories contribute 519 files each to the
   lodash tallies); repository counts are reported alongside file counts
   throughout for that reason.
5. **The C++ regexps bound rather than pinpoint the idiom.** `find_if(…rbegin`
   counts only same-line occurrences (multi-line call sites are missed → a
   floor), while `while (… .back())` also matches loops unrelated to
   trailing trimming (→ leans high). Together with the exact
   `find_last_not_of` counts, they establish the hand-rolled pattern's scale:
   thousands to tens of thousands of C++ files.
6. **Stream evidence files were size-trimmed** after capture (repeated
   `filters` events dropped, final one kept; originals' sizes and SHA-256
   hashes are recorded in `raw/STREAM_TRIM_MANIFEST.json`). All counts and
   language tallies were re-verified against the trimmed files and match.

## Bottom line for the paper

The operation the paper proposes is not speculative: three independent
ecosystems (Haskell, Kotlin/Ramda, lodash) ship it under three different
names and it is used in **thousands of files across hundreds-to-thousands of
repositories** in each; its string analogue is among the most-used string
operations in public code (tens of thousands of files, `rstrip` beyond
100,000); and C++ — which has no spelling for it (0 files) — currently
expresses it through reverse-iterator searches and `back()` loops in
**tens of thousands of files**. All figures are from the CSV and are stated
with their bounds; nothing above is extrapolated.
