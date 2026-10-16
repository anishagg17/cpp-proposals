# drop_last_while / take_last_while — WG21 Motivation Research

**Date of research:** 2026-10-15
**Proposed adaptors:** `std::ranges::views::drop_last_while(pred)` — remove the maximal trailing run of elements satisfying *pred*; `std::ranges::views::take_last_while(pred)` — retain only that maximal trailing run.
**Context:** P2760 lists both adaptors as Tier 1, each annotated "(not in range-v3)". SG9 reviewer Hewill Kang asked the author to explain why these views merit standardisation and to research use cases, including frequency in GitHub projects and what other languages provide. This report is written so its findings can be adapted into the motivation section of a WG21 paper. P2760R1 itself was opened and verified for the Tier 1 / not-in-range-v3 status: https://www9.open-std.org/JTC1/SC22/WG21/docs/papers/2023/p2760r1.html

**Confidence labels used throughout:** **VERIFIED** = read on the cited page/index on 2026-10-15; **APPROXIMATE** = real measurement with known distortions; **NOT RELIABLY MEASURABLE** = no trustworthy count could be obtained.

**A note on the two counting passes.** This report's counts are from a grep.app pass run on 2026-10-15. A second pass the same day (`frequency-study.md`) counted on Sourcegraph; in that pass the grep.app API was blocked (HTTP 429), and in this pass Sourcegraph returned a bot-verification page. The paper's §III.2 table combines one column from each pass; the columns are not a same-conditions comparison, and the paper states each ratio per index.

---

## Executive summary

1. **The operation is a named, standard facility in several major ecosystems, adopted independently.** Kotlin's standard library has shipped generic `dropLastWhile`/`takeLastWhile` since Kotlin 1.0 (for collections, arrays and `CharSequence`); Haskell's `base` has had `Data.List.dropWhileEnd` since base 4.5.0.0, with `takeWhileEnd` in `Data.Text`/`Data.ByteString`; Lodash has shipped `_.dropRightWhile`/`_.takeRightWhile` since 3.0.0; the pair also appears in Deno's standard library (`@std/collections`), Ramda, Remeda, es-toolkit, and Go's popular samber/lo library. This is convergent adoption, not one language's idiosyncrasy.
2. **Usage is measurable and non-trivial.** On grep.app (public-GitHub index, counts = files containing at least one match, read 2026-10-15): `dropLastWhile` 1,744 files; `takeLastWhile` 623; `dropWhileEnd` 626; `takeWhileEnd` 152; Lodash spellings `dropRightWhile`/`takeRightWhile` ≈3,138/≈3,024 (APPROXIMATE — inflated by vendored copies). GitHub Code Search itself requires sign-in and Sourcegraph blocked automated access in this pass (a separate Sourcegraph pass ran the same day; see the note above), so an exact GitHub-wide count is **NOT RELIABLY MEASURABLE** from a signed-out session; grep.app is the reproducible substitute and its universe is stated with every number.
3. **Where no direct API exists, the same need is met by a small set of recurring manual idioms** — reverse-iterator `find_if`/`find_if_not`, end-index while loops, `find_last_not_of` (7,928 files on grep.app), and string-specific trim operations (`trim_end`, `rstrip`, `rtrim`, `StripTrailingAsciiWhitespace`). Abseil's own `StripTrailingAsciiWhitespace` is implemented as literally a reverse-iterator `find_if_not`. The idiom evidence is *indirect* (it counts a string-specific special case), but it is broad and consistent.
4. **C++ is the outlier.** The standard library offers only the prefix `views::drop_while`/`take_while` plus the string-specific `find_last_not_of`; range-v3 has count-based `drop_last`/`take_last` but not the predicate forms (exactly as P2760 records). Boost.StringAlgorithms offers predicate trim (`trim_right_copy_if`) but as eager string algorithms, not lazy views. LLVM's `StringRef` has predicate `drop_while`/`take_while` at the front, count-based `drop_back`/`take_back`, char-set `rtrim` and `find_last_not_of` — everything except the predicate end-while operation.
5. **The reverse-composition workaround is real but lossy.** `reverse | drop_while | reverse` requires a bidirectional (and, to keep normal range properties, common) range; a single `reverse | take_while` yields elements in reversed order terminated by a sentinel; predicate evaluation order is reversed, which is observable for stateful predicates; and in lazy languages the composition destroys the laziness that Haskell's `dropWhileEnd` preserves. Haskell's own ecosystem (Cabal, GHC) keeps hand-optimised `dropWhileEndLE`/`takeWhileEndLE` functions whose comments state the reverse specification is slower and harder to read.
6. **Weaknesses the paper must concede:** `take_last_while` is consistently rarer than `drop_last_while` (roughly 1:3 to 1:4 in the counts); no C++-specific usage count can exist for a feature C++ does not have, so demand must be argued cross-language plus from C++ manual idioms; and no C++ benchmark of a dedicated view versus the composition was found. Section 4 lists the gaps SG9 is likely to probe and the evidence that would close them.

---

## Findings

### 1. Frequency in real code

**Index and method.** Counts below were read live on 2026-10-15 from grep.app (https://grep.app/search?q=dropLastWhile etc., one query per spelling). grep.app indexes public GitHub repositories; its headline figure is the number of **files** containing at least one match (per-file match counts are shown separately). Counts are not deduplicated: vendored copies, documentation, tests and type-definition files are included, and language facet counts are rounded in the UI. Each number is therefore exact *for grep.app's universe and definition*, and is labelled VERIFIED (single index) on that basis — not as a count of "uses" or "projects".

| Query (exact) | Count | Unit | Source / universe | Date | Confidence |
|---|---|---|---|---|---|
| `dropLastWhile` | 1,744 | files with ≥1 match | grep.app, public GitHub repos | 2026-10-15 | VERIFIED (single index) |
| `takeLastWhile` | 623 | files with ≥1 match | grep.app, public GitHub repos | 2026-10-15 | VERIFIED (single index) |
| `dropWhileEnd` | 626 | files with ≥1 match | grep.app, public GitHub repos | 2026-10-15 | VERIFIED (single index) |
| `takeWhileEnd` | 152 | files with ≥1 match | grep.app, public GitHub repos | 2026-10-15 | VERIFIED (single index) |
| `dropRightWhile` | 3,138 | files with ≥1 match | grep.app, public GitHub repos | 2026-10-15 | APPROXIMATE (vendored Lodash inflates: path facets included `node_modules/` 234, `dist/` 185, `docs/` 181) |
| `takeRightWhile` | 3,024 | files with ≥1 match | grep.app, public GitHub repos | 2026-10-15 | APPROXIMATE (same vendoring) |
| `find_last_not_of` (indirect, C++) | 7,928 | files with ≥1 match | grep.app, public GitHub repos | 2026-10-15 | VERIFIED (single index); **indirect** evidence — string-specific analogue, includes standard-library header/test files |
| `trim_end` (indirect, noisy) | 35,018 | files with ≥1 match | grep.app, public GitHub repos | 2026-10-15 | APPROXIMATE and **indirect** — includes unrelated identifiers (e.g. `trim_end_ms`) |
| `dropLastWhile` on GitHub Code Search | — | — | https://github.com/search?q=dropLastWhile&type=code requires sign-in; no count obtainable signed out | 2026-10-15 | NOT RELIABLY MEASURABLE |
| `dropLastWhile` on Sourcegraph | — | — | https://sourcegraph.com/search?q=context:global+dropLastWhile&patternType=keyword returned a bot-verification page | 2026-10-15 | NOT RELIABLY MEASURABLE |
| `dropLastWhile` on searchcode | — | — | classic global code index no longer offered at the site; no global count obtained | 2026-10-15 | NOT RELIABLY MEASURABLE |

**Reading of the direct-API rows.** The Kotlin spellings are the cleanest direct signal (Kotlin facet ≈1k of the 1,744 `dropLastWhile` files; top repositories included JetBrains/kotlin itself). The Haskell spellings are dominated by Haskell files (562 of 626 for `dropWhileEnd`; top repositories ghc/ghc, pandoc). The Lodash spellings have the largest raw counts but the worst vendoring distortion and should be quoted, if at all, as "≈3k files including vendored copies". Two robust qualitative facts survive every caveat: (a) both operations are used in thousands of files across at least four language ecosystems under stable, discoverable names; (b) the drop side is used ≈2.8× to ≈4.1× as often as the take side under each spelling pair (1,744:623; 626:152; ≈3,138:≈3,024 is the exception, and it is the vendoring-distorted pair — in non-vendored JavaScript usage via Ramda/Remeda/Deno names the drop side likewise dominates).

**Manual implementations of the same operation (indirect evidence).** Where the direct API is absent, the operation is hand-written in a few stereotyped forms, all observed on 2026-10-15:

- **Reverse iteration + find_if / find_if_not.** The canonical C++ string trim (a widely copied gist, https://gist.github.com/alibitek/09f36d7d7c8896a5d15d) erases from `find_if(s.rbegin(), s.rend(), …isspace…).base()` to `s.end()`. Abseil's production `StripTrailingAsciiWhitespace` is the same idiom verbatim inside a major library (source: https://chromium.googlesource.com/chromium/src/third_party/abseil-cpp/+/refs/heads/main/absl/strings/ascii.h).
- **While loops decrementing an end index / popping from the back.** TutorialsPoint's C++ trailing-zeros article shows `while (i >= 0 && str[i] == '0') --i;` alongside the `find_last_not_of` form (https://www.tutorialspoint.com/article/remove-trailing-zeros-from-string-in-cplusplus). Google's protobuf Objective-C generator drops trailing empty comment lines with `while (!raw_lines.empty() && raw_lines.back().empty()) raw_lines.pop_back();` (https://fuchsia.googlesource.com/third_party/protobuf/+/0196d1e1387a903ee0cc44b301bb94f7ce7d5d2a/src/google/protobuf/compiler/objectivec/helpers.cc).
- **`find_last_not_of` and analogues.** `std::string`/`string_view::find_last_not_of` is the standard C++ mechanism behind trailing trims (7,928 grep.app files, above); LLVM's `StringRef` independently provides `find_last_not_of` overloads (https://docs.hdoc.io/hdoc/llvm-project/rECFCAF624C04523A.html).
- **trim_end / trim_right / rstrip family.** Rust `str::trim_end`/`trim_end_matches`, Python `str.rstrip`, PHP `rtrim`, C# `String.TrimEnd`, Qt `QString::trimmed`, Abseil `StripTrailingAsciiWhitespace` — all string/character-specific special cases of drop_last_while (see §2). The noisy 35,018-file `trim_end` count shows the *vocabulary* is everywhere even though it cannot be read as a precise usage count.
- **Domain-specific trailing erasure.** Trailing zeros (numeric formatting), trailing NULs/whitespace/newlines (text and protocols), trailing separators and empty split fields (paths, URLs, cookies), trailing default/sentinel values — see the catalogue in §3.

**What the frequency evidence does not show.** It does not isolate C++ demand (the feature does not exist in C++ to be counted); it does not deduplicate vendored code; and grep.app's universe, while large, is not all of GitHub. The numbers support "recurring, cross-ecosystem, named demand", not a precise market size.

### 2. Other languages and libraries

Classification key: **Direct** = generic sequence operation under a specific name; **Direct (strings)** = string/text types only; **Composed** = documentation/idiom is reverse + take/drop_while; **String-specific** = character-set trim only; **Absent** = no facility found in the standard library.

| Language / library | Facility | Class | Source |
|---|---|---|---|
| Kotlin stdlib (since 1.0) | `dropLastWhile` / `takeLastWhile` for collections, arrays, `CharSequence` | Direct | https://kotlinlang.org/docs/collection-parts.html ; https://kotlinlang.org/api/core/kotlin-stdlib/kotlin.text/drop-last-while.html ; https://kotlinlang.org/api/core/kotlin-stdlib/kotlin.text/take-last-while.html |
| Haskell `base` (since 4.5.0.0) | `Data.List.dropWhileEnd`; **no** `takeWhileEnd` in `Data.List` | Direct (drop only) | Data.List documentation (extract: https://github.com/pushcx/hpffp-resources/raw/refs/heads/master/Chapter%209/Data.List.pdf) |
| Haskell `text` / `bytestring` / `extra` | `takeWhileEnd` (and `dropWhileEnd`) on `Text`/`ByteString`; `Data.List.Extra` both | Direct (strings/bytes; extra: lists) | bytestring documentation (defines it via reverse∘takeWhile∘reverse specification), via grep.app/Hackage reads 2026-10-15 |
| JavaScript — Lodash (since 3.0.0) | `_.dropRightWhile` / `_.takeRightWhile` | Direct | https://lodash.com/docs/4.18.1 |
| JavaScript/TypeScript — Ramda, Remeda, es-toolkit | `dropLastWhile`/`takeLastWhile` (Ramda, Remeda); `dropRightWhile`/`takeRightWhile` (es-toolkit, Lodash-compatible) | Direct | observed in grep.app results 2026-10-15 |
| TypeScript — Deno standard library | `@std/collections` `dropLastWhile` / `takeLastWhile` | Direct | https://jsr.io/@std/collections/doc/all_symbols |
| Go — samber/lo (ecosystem; stdlib absent) | `lo.DropLastWhile` (with a documented allocation caveat for long matches) | Direct (third-party) | grep.app result for samber/lo `it/seq.go`, 2026-10-15 |
| Scala stdlib | count-based `dropRight`/`takeRight` and prefix `dropWhile`/`takeWhile` only; no `dropRightWhile`/`takeRightWhile` found (2.13 Seq API listing checked; 3.3 LTS views expose count-based `DropRight`) | Absent / Composed | https://github.com/scala/bug/issues/4247 ; https://www.scala-lang.org/files/archive/api/3.3_LTS/scala/collection/View$$DropRight.html |
| Swift + swift-algorithms | Swift stdlib has the front forms (`prefix(while:)`, `drop(while:)`) only; Apple's swift-algorithms package provides `suffix(while:)` (take side) and `trimmingSuffix(while:)`, both on `BidirectionalCollection` | Direct (both sides, Apple library) | swift-algorithms Trim and Suffix guides (see Sources); https://forums.swift.org/t/should-there-be-bidirectionalcollection-droplast-while/48059 |
| Rust stdlib | `str::trim_end`, `str::trim_end_matches` (pattern-based, strings); slices/iterators: prefix `take_while`/`skip_while` only; a generic slice version was accepted by the libs-api team in September 2024 but is not yet stable | String-specific / Absent (generic, stable) | https://github.com/rust-lang/libs-team/issues/414 |
| Python stdlib | `itertools.dropwhile`/`takewhile` (prefix only); `str.rstrip` (character set) | Absent (generic) / String-specific | https://docs.python.org/3.14/library/itertools.html |
| Ruby stdlib | `Enumerable#drop_while`/`take_while` (prefix only); end operation hand-written via reverse or a pop-while loop | Absent / Composed | https://www.exchangetuts.com/is-there-a-ruby-idiom-for-popping-items-from-an-array-while-a-condition-is-true-1640727963961766 |
| C# / .NET | LINQ `TakeWhile`/`SkipWhile` (prefix), `TakeLast(n)`/`SkipLast(n)` (count); `String.TrimEnd` (character set) | Absent / String-specific | Microsoft Learn, `Enumerable.TakeWhile` / `Enumerable.SkipWhile` pages, accessed 2026-10-15 |
| Java | Streams `takeWhile`/`dropWhile` since Java 9 (prefix only); Kotlin's own Java-comparison guide lists `takeLastWhile`/`dropLastWhile` among operations missing from Java's standard library | Absent | https://kotlinlang.org/docs/java-to-kotlin-collections-guide.html |
| PHP | `rtrim($str, $charlist)` (character set, strings); array versions only in Lodash-style ports | String-specific | PHP documentation via grep.app/tutorial reads 2026-10-15 |
| Elixir | `Enum`/`Stream` `take_while`/`drop_while` (prefix); negative-count `drop`/`take` from end; no *-while end variants found in HexDocs | Absent | HexDocs reads 2026-10-15 (older versions surfaced; "not found", not exhaustively proven) |
| Clojure | `take-while`/`drop-while` (prefix); `take-last(n)`/`drop-last(n)` (count) | Absent | https://clojuredocs.org/clojure.core/drop-while (community docs) |
| D (Phobos) | Official docs for `std.algorithm.searching.find` instruct users to search from the end via `find!pred(retro(haystack))` — the reverse composition is the documented idiom | Composed (documented) | https://dlang.org/library/std/algorithm/searching/find.html |

**Pattern across the table.** Languages that added the facility made it *generic over sequences* (Kotlin, Lodash/Ramda, Deno std, Haskell text/bytestring) and have kept it for a decade or more (Haskell 2012, Lodash 2015, Kotlin 2016). Languages without it consistently strand users at string-specific trims or at reverse compositions — and two ecosystems show documented, recurring requests for the missing piece (Scala issue #4247; the Swift Forums thread in which hand-written `dropLast(while:)` implementations contained off-by-one errors that other participants corrected).

### 2b. C++ libraries and the existing standard library

| Facility | What exists | Gap |
|---|---|---|
| C++ standard library | `views::reverse`, `views::drop_while`, `views::take_while` (prefix only); `basic_string`/`basic_string_view::find_last_not_of` (string-specific) | No end-while view; no generic predicate trim |
| range-v3 | count-based `views::drop_last(n)` / `take_last(n)` (documented example: `vec | rv::drop_last(2)`; `drop_last` requires a sized source) — https://lukasfro.github.io/range-v3-doc/chapter_04/04_01_views.html | Predicate forms absent — exactly P2760's "(not in range-v3)" annotation |
| Boost.StringAlgorithms | `boost::algorithm::trim_right_copy_if` / `trim_right_if` — arbitrary unary predicate, "Right trim - parametric" (Boost 1.82 docs: https://original.boost.org/doc/libs/1_82_0/doc/html/boost/algorithm/trim_right_copy_if.html) | Eager algorithms over sequences/strings (copy or in-place erase), string-library scope; not lazy, composable range views |
| LLVM `StringRef` | predicate `drop_while`/`take_while` (front), count-based `drop_back`/`take_back`, char/char-set `rtrim`, `find_last_not_of` — https://docs.hdoc.io/hdoc/llvm-project/rECFCAF624C04523A.html | No predicate end-while; string-specific |
| Abseil | `absl::StripTrailingAsciiWhitespace`, `StripPrefix`/`StripSuffix` (exact affixes) — https://chromium.googlesource.com/chromium/src/third_party/abseil-cpp/+/refs/heads/main/absl/strings/ascii.h | Whitespace/string-specific; implemented internally as reverse-iterator `find_if_not`, i.e. the manual idiom productised for one predicate |
| Qt | `QString::trimmed()` (both ends, whitespace, strings) | No predicate or generic end-while adaptor found |
| Folly | Not verified in this research pass (no authoritative page captured) | Do not cite either way |

### 3. Concrete use cases

Each entry: **class** (drop / take / both), domain, one-sentence explanation, source. Entries sourced "via grep.app" were observed in grep.app result pages on 2026-10-15 (query URLs in §1).

| # | Example | Class | Domain | Explanation | Source |
|---|---|---|---|---|---|
| 1 | Kotlin `split(...).dropLastWhile { it.isEmpty() }` in SmartDock, Automattic Pocket Casts (`IntentUtil.kt`, splitting a URL on "&"), EhViewer (cookie/content-type parsing) | drop | Text, URLs, parsers | Splitting text produces spurious trailing empty fields; the idiom discards exactly the trailing empty run — the single most repeated concrete pattern in the Kotlin results. | grep.app, `dropLastWhile`, 2026-10-15 |
| 2 | Sentry web UI: `dropRightWhile(widths, w => w === COL_WIDTH_UNDEFINED)` (`useQueryBasedColumnResize.tsx`) | drop | UI data | Trailing table columns whose widths were never defined are dropped from the widths array before layout. | grep.app, `dropRightWhile`, 2026-10-15 |
| 3 | GHC's vendored filepath library: `dropWhileEnd isPathSeparator`, `dropWhileEnd (== ' ')` | drop | Paths | Trailing path separators and trailing spaces in file names are stripped generically by end-while. | grep.app, `dropWhileEnd`, 2026-10-15 |
| 4 | Haskell WAI (`Network/Wai/Util.hs`): `trimWS` built from `dropWhileEnd (== _space)` | drop | HTTP / text protocols | Trailing whitespace is removed from header-ish byte strings as one half of a trim. | grep.app, `dropWhileEnd`, 2026-10-15 |
| 5 | Pandoc: `T.takeWhileEnd (/= ':')` on tag names; `T.takeWhileEnd (/= '\n')`; `T.takeWhileEnd (/= '.')` | take | Parsers, suffix extraction | The suffix after the last ':' (namespace-local name), the last line of a text, and the suffix after the last '.' (module name) are each exactly a maximal trailing run. | grep.app, `takeWhileEnd`, 2026-10-15 |
| 6 | Wasp: `takeWhileEnd (/= ':')` on the first line of Docker port output | take | Tooling output parsing | The port is the trailing run of non-':' characters in `docker` output. | grep.app, `takeWhileEnd`, 2026-10-15 |
| 7 | NumPy `numpy.polynomial.polynomial.polytrim` / `trimcoef` (and `chebtrim`) | drop | Numeric | Trailing (highest-order) polynomial coefficients that are zero or below a tolerance — a predicate, not a fixed value — are removed. | https://numpy.org/doc/stable/reference/generated/numpy.polynomial.polynomial.polytrim.html |
| 8 | TutorialsPoint C++ trailing-zeros walkthrough (manual `while` end-index loop and `find_last_not_of('0')` + erase forms) | drop | Numeric formatting | Removing trailing zeros from a formatted number is the textbook C++ instance of the manual idiom. | https://www.tutorialspoint.com/article/remove-trailing-zeros-from-string-in-cplusplus |
| 9 | LibreOffice `rtl::math` string-to-number formatting, parameter `bEraseTrailingDecZeros` | drop | Numeric formatting | A mature office suite exposes "erase trailing decimal zeros" as a named formatting option. | https://api.libreoffice.org/docs/cpp/ref/a00143.html |
| 10 | Google protobuf Objective-C generator: pop-back loop removing trailing empty comment lines; `absl::StripTrailingAsciiWhitespace` per line | drop | Code generation, records/text | Generated documentation comments must not end with empty lines or trailing whitespace; both halves are hand-written end-while operations. | https://fuchsia.googlesource.com/third_party/protobuf/+/0196d1e1387a903ee0cc44b301bb94f7ce7d5d2a/src/google/protobuf/compiler/objectivec/helpers.cc |
| 11 | Lodash documentation's canonical example: array of user objects with `active` flags | both | Containers / records | `takeRightWhile` keeps the trailing run of active users and `dropRightWhile` removes it — the example Lodash itself chose to define the operations. | https://lodash.com/docs/4.18.1 |
| 12 | Kotlin documentation's canonical example: `"<<<First Grade>>>"` with predicate `!it.isLetter()` | both | Text | `dropLastWhile` yields `"<<<First Grade"` and `takeLastWhile` yields `">>>"` — the pair partitions a string at the trailing non-letter run. | https://kotlinlang.org/api/core/kotlin-stdlib/kotlin.text/drop-last-while.html |
| 13 | Cabal / GHC `dropWhileEndLE`, `takeWhileEndLE` (low-level variants in `Distribution/Utils/Generic.hs`; filepath code in GHC) | both | Paths, text, performance | Two core Haskell codebases maintain hand-optimised end-while functions rather than using the reverse specification, with comments citing speed and readability. | grep.app, `dropWhileEnd`, 2026-10-15 |
| 14 | Classic C++ trim gist: erase from `find_if(s.rbegin(), s.rend(), …).base()` | drop | Text (manual idiom) | The form in which C++ programmers actually write drop_last_while for whitespace today. | https://gist.github.com/alibitek/09f36d7d7c8896a5d15d |

**Domain coverage note (gap).** Text/parsers, paths/URLs, numeric formatting, containers/config, UI data, suffix extraction and code generation are evidenced above. A comparably strong *primary-source* example in binary protocols (e.g. stripping trailing padding/NUL bytes from a fixed-size record) was **not** captured in this pass; the pattern is the obvious analogue of the trailing-NUL/whitespace cases, but the paper should either find a citable instance or not claim the domain.

### 4. Standardisation argument and counterarguments

**The objection.** "`reverse | drop_while | reverse` (resp. `take_while`) already expresses this; a new view is mere symmetry."

**What the evidence supports in reply.**

1. **The composition restricts the range category.** `reverse_view` requires the underlying range to model `bidirectional_range` (cppreference: https://en.cppreference.com/cpp/ranges/reverse_view). A dedicated end-while adaptor is not forced to impose bidirectionality in the same way — for example it can locate the boundary by scanning and caching, admitting forward ranges at the cost of a cache — so the composition silently excludes range types a purpose-built view need not. (Stated carefully: this is a design-space argument; the paper must specify the adaptor's own requirements rather than merely assert superiority.)
2. **The take side does not survive a single reverse.** `views::take_while` terminates its range with a sentinel, not an iterator; `reverse | take_while(pred)` therefore yields the trailing run *in reverse order* as a sentinel-terminated view. Recovering the original order needs the second reverse, stacking three adaptors, with the resulting view's properties (common-range, sized, const-iterability) only as good as the weakest link — `drop_while_view`, for instance, is not const-iterable and models common/sized ranges only under narrower conditions than its underlying range (Microsoft Learn `drop_while_view` class page, accessed 2026-10-15).
3. **Predicate evaluation order is observable.** The composition applies *pred* to elements in reverse order relative to the original sequence — the last element is tested first. For pure predicates this is invisible; for stateful, counting, short-circuiting or expensive predicates it changes behaviour and cost profile relative to a specification written in terms of the trailing run.
4. **Laziness is not preserved in general.** Haskell's `dropWhileEnd` is specified and implemented to remain lazy in the spine in cases where `reverse . dropWhile p . reverse` cannot produce output at all (e.g. infinite lists) — a point made explicitly by the `utility-ht` package, which provides a strict-element variant precisely because the reverse specification fails there (https://hackage.haskell.org/package/utility-ht-0.0.15/candidate/docs/Data-List-Reverse-StrictElement.html). In C++ terms: the composition forces full bidirectional traversability even when the consumer only needs the boundary.
5. **Implementations of a lazy end-while are genuinely non-trivial.** Ramda's transducer for the operation (`_xdropLastWhile`) must retain candidate elements in an internal buffer until the predicate fails — evidence that a correct, lazy, single-pass end-while is a designed component, not a one-line composition, in any language.
6. **Performance evidence from production Haskell.** Cabal and GHC maintain `dropWhileEndLE`/`takeWhileEndLE` whose comments describe the reverse form as the specification but the hand-written version as considerably faster (and easier to read); GHC's filepath code uses the optimised form for stripping trailing space from path names and lines of text. This is Haskell-side evidence; the paper should not present it as a C++ benchmark (none was found).
7. **Error-proneness of hand composition.** In the Swift Forums discussion, participants' hand-written `dropLast(while:)` implementations contained off-by-one errors corrected in-thread; the Ruby Q&A shows users reduced to reverse-and-reverse-back recipes and hand loops. Recurring hand-implementations of a boundary-finding operation are exactly where standardisation removes a class of small errors.

**Demand vs. mere symmetry.** The demand evidence is: (i) independent standard-library adoption (Kotlin 1.0, Haskell base 2012 for the drop side, Deno std) and decade-long retention; (ii) repeated re-implementation in ecosystem libraries (Lodash, Ramda, Remeda, es-toolkit, samber/lo, swift-algorithms, Haskell extra/text/bytestring); (iii) thousands of files of usage under stable names (§1); (iv) the ubiquity of the manual idiom where the API is missing, including inside Abseil and protobuf. The symmetry-only evidence (Scala issue #4247, requesting the end-while counterpart to `dropWhile`; Swift forum requests) is real but weaker, and the paper should cite it as corroboration, not as the lead argument. Note also the asymmetry the paper must own: Haskell's `base` standardised only the drop side for lists; the take side's precedent rests on text/bytestring, Kotlin and the JS libraries.

**Gaps SG9 is likely to challenge, and what would close them.**

- *"How much is this used in C++?"* — Unanswerable by direct count (feature absent). Strongest available proxies: `find_last_not_of` at 7,928 files and the trim-idiom literature. A signed-in GitHub Code Search count for the C++ idiom spellings, or a GH Archive/BigQuery study, would materially strengthen this.
- *"Why is take_last_while 3–4× rarer?"* — Concede the ratio; justify the take side on suffix-extraction cases (Pandoc/Wasp, §3 #5–6) and on partition symmetry with the drop side once either is standardised.
- *"grep.app is one index."* — True of this pass; the six spellings were counted again on Sourcegraph the same day (`frequency-study.md`). The two indexes differ in universe and method, so the paper reports both columns side by side rather than merging them, and states ratios per index.
- *"Show me the C++ cost of the composition."* — No C++ benchmark was found. An implementation of both views (e.g. as a range-v3-style prototype) with micro-benchmarks against the triple-reverse composition on representative ranges would close this.
- *Design questions this research does not answer:* laziness/caching semantics for forward ranges; whether the views should model `borrowed_range`, sized and common ranges and under what conditions; predicate invocation guarantees (order, count); naming (`drop_last_while` vs `drop_while_end` given Haskell's prior art). These are for the paper's design section, informed by P2760 and the SG9 follow-on papers visible in the 2026-07 mailing (https://wg21.org/mailing/?mailing=2026-07&group=sg9).

---

## Paper-ready motivation bullets

- The operation "remove/retain the maximal trailing run satisfying a predicate" is a **named standard-library facility in Kotlin (since 1.0), Haskell (drop side in `base` since 2012; both sides in `text`/`bytestring`), Lodash (since 3.0.0) and Deno's standard library**, and is independently provided by Ramda, Remeda, es-toolkit, samber/lo (Go) and Apple's swift-algorithms — convergent, decade-long adoption across ecosystems.
- Measured on grep.app's public-GitHub index on 2026-10-15, the direct APIs appear in **1,744 (`dropLastWhile`), 623 (`takeLastWhile`), 626 (`dropWhileEnd`) and 152 (`takeWhileEnd`) files**, with the Lodash spellings in roughly 3k files each including vendored copies — recurring production use, not toy examples.
- The recurring concrete patterns are mundane and universal: **discarding trailing empty fields after `split`** (Kotlin codebases incl. Pocket Casts), **stripping trailing path separators and spaces** (GHC filepath), **trimming trailing whitespace in protocol text** (WAI), **dropping trailing undefined column widths** (Sentry UI), **removing trailing zero/tolerance-small polynomial coefficients** (NumPy `polytrim`), and **extracting suffixes** — the part after the last ':', '.' or newline — in parsers (Pandoc, Wasp).
- Where the generic operation is missing, programmers write it by hand in stereotyped, error-prone forms — reverse-iterator `find_if`, end-index while loops, pop-back loops — **including inside Abseil (whose `StripTrailingAsciiWhitespace` *is* a reverse-iterator `find_if_not`) and Google's protobuf code generators**; C++'s own string-specific crutch, `find_last_not_of`, appears in ~7.9k files.
- C++ today offers only the prefix `drop_while`/`take_while` views and count-based `drop_last`/`take_last` in range-v3; **P2760 already ranks the predicate pair Tier 1 and notes they are not in range-v3** — the hole is in the ranges vocabulary, not in any one string type, which is why string-specific trims (Boost, Qt, LLVM, Abseil) have not closed it.
- The `reverse + drop_while/take_while + reverse` composition **requires bidirectional ranges, reverses the order of predicate evaluation, yields a sentinel-terminated reversed view on the take side unless reversed again, and forfeits laziness** that other languages' native implementations preserve; production codebases that tried the composition (Cabal, GHC) replaced it with hand-optimised implementations for speed and clarity.

---

## Could not verify / Limitations and unresolved questions

- **Cross-index replication was partial:** a second pass (`frequency-study.md`) counted the same spellings on Sourcegraph the same day; in that pass grep.app was API-blocked, and in this pass Sourcegraph served a bot-verification page, so each index's numbers rest on a single pass. GitHub Code Search requires sign-in and searchcode no longer offers its classic global index, so no third count exists. All §1 counts are single-index (grep.app) and are labelled as such. **NOT RELIABLY MEASURABLE:** an exact GitHub-wide count for any spelling.
- **Scala status is bounded, not exhaustive:** absence of `dropRightWhile`/`takeRightWhile` was established from the Scala 2.13 Seq API listing and the 3.3 LTS view documentation plus the long-standing feature issue; a member-by-member audit of Scala 3.x was not performed.
- **Elixir status is "not found"**, based on HexDocs pages that surfaced in older versions; not a proof of absence in the current release.
- **Folly** trim facilities were **not verified** (no authoritative page captured); the report makes no claim about Folly either way.
- **Clojure** was sourced to community documentation (ClojureDocs), not official references.
- **No binary-protocol primary-source use case** was captured (§3 note); trailing-padding erasure is asserted nowhere in this report as evidenced.
- **No C++ benchmark** comparing a dedicated view with the reverse composition was found; performance claims are limited to Haskell-side comments in Cabal/GHC sources.
- grep.app counts include tests, documentation and vendored copies and are **not** deduplicated; Lodash-spelling counts are labelled APPROXIMATE for this reason. Language facet subtotals in the grep.app UI are rounded.
- This research does not settle design questions (forward-range caching, borrowed/sized/common guarantees, predicate evaluation guarantees, naming) — see §4.

## Sources

All sources accessed 2026-10-15. Index counts: `notes/frequency-grepapp-2026-10-15.md`; C++ library verification details: `notes/cpp-libs-verification-2026-10-15.md` (both in this research directory).

- P2760R1 (Tier 1 status, "not in range-v3"): https://www9.open-std.org/JTC1/SC22/WG21/docs/papers/2023/p2760r1.html
- WG21 mailings, SG9, 2026-07 (follow-on Tier 1 papers): https://wg21.org/mailing/?mailing=2026-07&group=sg9
- grep.app queries: https://grep.app/search?q=dropLastWhile , https://grep.app/search?q=takeLastWhile , https://grep.app/search?q=dropWhileEnd , https://grep.app/search?q=takeWhileEnd , https://grep.app/search?q=dropRightWhile , https://grep.app/search?q=takeRightWhile , https://grep.app/search?q=find_last_not_of , https://grep.app/search?q=trim_end
- Blocked cross-checks: https://github.com/search?q=dropLastWhile&type=code ; https://sourcegraph.com/search?q=context:global+dropLastWhile&patternType=keyword
- Kotlin: https://kotlinlang.org/docs/collection-parts.html ; https://kotlinlang.org/api/core/kotlin-stdlib/kotlin.text/drop-last-while.html ; https://kotlinlang.org/api/core/kotlin-stdlib/kotlin.text/take-last-while.html ; https://kotlinlang.org/docs/java-to-kotlin-collections-guide.html
- Haskell: https://github.com/pushcx/hpffp-resources/raw/refs/heads/master/Chapter%209/Data.List.pdf ; https://hackage.haskell.org/package/utility-ht-0.0.15/candidate/docs/Data-List-Reverse-StrictElement.html
- Lodash docs (4.18.1): https://lodash.com/docs/4.18.1
- Deno std (JSR symbols): https://jsr.io/@std/collections/doc/all_symbols
- Scala: https://github.com/scala/bug/issues/4247 ; https://www.scala-lang.org/files/archive/api/3.3_LTS/scala/collection/View$$DropRight.html
- Swift: https://forums.swift.org/t/should-there-be-bidirectionalcollection-droplast-while/48059 ; swift-algorithms Guides: https://github.com/apple/swift-algorithms/blob/main/Guides/Trim.md ; https://github.com/apple/swift-algorithms/blob/main/Guides/Suffix.md
- Rust proposal: https://github.com/rust-lang/libs-team/issues/414
- Python itertools: https://docs.python.org/3.14/library/itertools.html
- Ruby idiom Q&A: https://www.exchangetuts.com/is-there-a-ruby-idiom-for-popping-items-from-an-array-while-a-condition-is-true-1640727963961766
- D Phobos `find` (retro idiom): https://dlang.org/library/std/algorithm/searching/find.html
- Clojure (community docs): https://clojuredocs.org/clojure.core/drop-while
- range-v3 views docs (mirror): https://lukasfro.github.io/range-v3-doc/chapter_04/04_01_views.html
- Boost 1.82, `trim_right_copy_if`: https://original.boost.org/doc/libs/1_82_0/doc/html/boost/algorithm/trim_right_copy_if.html
- LLVM `StringRef` (LLVM 15 docs mirror): https://docs.hdoc.io/hdoc/llvm-project/rECFCAF624C04523A.html
- Abseil `ascii.h` (Chromium mirror): https://chromium.googlesource.com/chromium/src/third_party/abseil-cpp/+/refs/heads/main/absl/strings/ascii.h
- protobuf Objective-C helpers (Fuchsia mirror, pinned commit): https://fuchsia.googlesource.com/third_party/protobuf/+/0196d1e1387a903ee0cc44b301bb94f7ce7d5d2a/src/google/protobuf/compiler/objectivec/helpers.cc
- cppreference, `reverse_view`: https://en.cppreference.com/cpp/ranges/reverse_view
- NumPy `polytrim`: https://numpy.org/doc/stable/reference/generated/numpy.polynomial.polynomial.polytrim.html
- LibreOffice `rtl::math`: https://api.libreoffice.org/docs/cpp/ref/a00143.html
- TutorialsPoint, trailing zeros in C++: https://www.tutorialspoint.com/article/remove-trailing-zeros-from-string-in-cplusplus
- C++ trim gist: https://gist.github.com/alibitek/09f36d7d7c8896a5d15d
- Microsoft Learn, `drop_while_view` / `Enumerable.TakeWhile` / `Enumerable.SkipWhile` class pages (accessed 2026-10-15; URLs recorded in session notes)
