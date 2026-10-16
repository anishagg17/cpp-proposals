# `std::views::trim` / `std::views::trim_while` — design research & duplicate check

**Note (2026-10-16):** this copy postdates the rename of the
predicate form from `trim_if` to `trim_while` (Arthur O'Dwyer's
feedback on the std-proposals float; the author agreed). The
research below was done on 13 October 2026, before the rename, and
its account of the naming decision (§4.1) records the position at
that time; the draft paper's §V.2 gives the current position and
the history. Everything else is unchanged.

**Purpose:** research backing for a first WG21 paper (target C++29; audience SG9 Ranges, then LEWG).
**Date of research:** 13 October 2026.
**Scope of this document:** (1) the exact P2760 plan entry this proposal implements; (2) prior art, with range-v3's semantics pinned down from source; (3) the duplicate-check evidence; (4) design decisions, each with evidence and a recommendation.
This is research only — no paper wording, no implementation (the implementation lives in `../impl/`).

**Bottom line up front**

- **The plan asks for this.** P2760R0, *A Plan for C++26 Ranges* (SG9's plan paper), lists `trim` as a range-v3 adaptor at **Tier 2**, and its §6.2 Tier 2 summary names `views::trim` among the Tier 2 range adaptors. No design discussion of trim appears anywhere else in P2760 — the plan entry is a table row plus the summary list, so the design work is genuinely open.
- **range-v3 is the prior art and its semantics are settled:** `views::trim(rng, pred)` requires a bidirectional, common range; `pred(e) == true` means "trim `e`"; the view's iterators are the base's own iterators, with the bounds found by a forward scan (`find_if_not`) and a backward scan; the view is a common range, preserves the base's iterator category up to contiguous, and is explicitly **not** a sized range in range-v3 (see §4.4 for why this proposal diverges on that one point, following `drop_while_view` instead).
- **No duplicate.** A fresh full-text duplicate check on 13 Oct 2026 found zero occurrences of "trim" in the complete official 2025 and 2026 WG21 papers indexes (§3). Nobody has papered this plan item yet — but the plan is being worked through quickly by others (see §3.3), so it is first-come.

---

## 1. The P2760 entry, exactly

P2760R0 (<https://wg21.link/p2760r0>), §2's table of range-v3 adaptors and their status/priority contains the row:

| View | Current Status | Proposed Priority |
|---|---|---|
| `trim` | range-v3 | Tier 2 |

and §6.2 ("Tier 2") lists, under "Range Adaptors", `views::trim` alongside `views::adjacent_filter`, `views::adjacent_remove_if`, `views::head`, `views::intersperse`, `views::iterate`, `views::split_when`, `views::tail`, and `views::unique`.

That is the entirety of what P2760 says about trim: there is no design section for it (unlike `cache_last` or `istream<T>`, which get subsections), no signature, and no stated steer on open questions such as forward-only ranges or the predicate-vs-value naming split. Any steer must therefore come from range-v3's shipped semantics and from SG9 discussion of this paper — the draft paper says so explicitly rather than inventing a plan position.

Context that matters for prioritisation: Tier 1 of the same plan is largely the take/drop family and simple compositions (`remove`/`remove_if`, `replace`/`replace_if`, `slice`, …), i.e. SG9's expressed direction is "finish porting the well-understood range-v3 adaptors". `trim` is squarely in that programme.

## 2. Prior art

### 2.1 range-v3 `views::trim` (primary prior art — semantics verified from source)

Source read directly: `include/range/v3/view/trim.hpp` and `test/view/trim.cpp` (range-v3 master, fetched 13 Oct 2026).

- **Signature.** `ranges::views::trim(rng, pred)` and the pipe form `rng | ranges::views::trim(pred)`. An optional third argument, a projection, exists in range-v3 (the predicate is composed with the projection); see §4.6 for why this proposal omits it in R0.
- **Predicate direction.** `pred(e) == true` means *e is trimmed*. In range-v3's own test, with `ia = {0,1,2,3,4,3,2,1,2,3,4,3,2,1,0}` and `p = [](int i){ return i < 4; }`, `views::trim(ia, p)` yields `{4,3,2,1,2,3,4}`.
- **Requirements.** The base must be a `viewable_range` modelling **`bidirectional_range` and `common_range`**, and `pred` must be an `indirect_unary_predicate` on its iterator. There is no forward-only mode (see §4.2).
- **Mechanism.** `trim_view` stores the base and the predicate, and caches both bounds (lazily, in non-propagating caches): `begin()` is `find_if_not(rng, pred)`; `end()` walks backwards from `ranges::end(rng)` past elements satisfying `pred`, stopping at `begin()`. The view's iterator type is exactly the base's iterator type.
- **Concept profile (asserted in range-v3's test).** `view_`, `common_range`, the base's category preserved (contiguous and random-access cases both tested), and **`!sized_range`** — the header explicitly specialises `disable_sized_range<trim_view> = true`. `enable_borrowed_range<trim_view>` propagates from the base.
- **Author/history.** The range-v3 header credits Johel Guerrero (2019). The facility has shipped in range-v3 since then, so there is roughly seven years of user experience behind the semantics above.

### 2.2 The string-trimming family (the operation's home territory)

Trimming began as a string operation and every mainstream ecosystem has it; the interesting variation is *what identifies a trimmable element* — a fixed whitespace set, a set of characters, or a predicate:

| Facility | Trims | Identification |
|---|---|---|
| Boost.StringAlgo `boost::algorithm::trim` / `trim_if` | both ends of a string/range | predicate (`trim_if`); `trim` uses a locale space predicate |
| Rust `str::trim` / `trim_matches` | both ends | `trim`: Unicode `Pattern_White_Space`; `trim_matches`: a char, a set of chars, or a predicate closure |
| Python `str.strip([chars])` | both ends (also `lstrip`/`rstrip`) | optional set of characters; whitespace by default |
| Kotlin `CharSequence.trim(predicate)` | both ends | a `(Char) -> Boolean` predicate (also `trim(chars…)`) |
| C# `String.Trim(params char[])` | both ends | a set of characters; whitespace by default |
| Java `String.strip()` | both ends | Unicode whitespace (`Character.isWhitespace`) |

C++ has Boost.StringAlgo's string-focused version but **no generic-range version in the standard library**: `std::string` has no trim member, and no range adaptor expresses it (see §2.3). The predicate form — Boost's `trim_if`, Kotlin's `trim(predicate)`, Rust's closure form of `trim_matches` — is the one that generalises to arbitrary ranges, and it is the form range-v3 shipped.

### 2.3 Why existing std adaptors do not already express this

- `views::drop_while(pred)` trims only the leading edge.
- The trailing edge cannot be obtained by composing one-sided adaptors without reversing twice *and copying*: `rng | reverse | drop_while(pred) | reverse` requires the reversed view to be materialised or re-walked, does not preserve the identity/positions of the underlying iterators, and composes terribly (`reverse` also requires bidirectional + common, so it buys nothing on requirements). range-v3's own test makes the same point by checking `trim` against a `drop_while | reverse | drop_while` composition for the leading+trailing *sets* of dropped elements.
- `views::take_while` / `drop` / `take` are position- or predicate-from-one-end only.
So a dedicated adaptor is the honest expression of "the subrange left after removing a matching prefix and suffix" — which is exactly the niche P2760 recorded.

## 3. Duplicate check

### 3.1 Method and evidence (13 Oct 2026)

A fresh duplicate check was run on 13 Oct 2026 over the **complete official 2025 and 2026 WG21 papers indexes** (the full wg21.link/open-std paper listings for both years), searching the full text for "trim". Result: **zero occurrences** — no paper in either index proposes, or even mentions, a trim adaptor. Sanity anchor: the same fetch-and-search method **does** find known papers in the same indexes (P4355, P4294 were used as positive controls), so the zero is a real zero for the method, not a fetch failure.

This check supersedes keyword web searches: the sibling pack in this workspace (saturating-cast FP) was burned by exactly a keyword-based sweep missing a paper that a full-index check would have caught, so full-index coverage is now the house standard.

### 3.2 Neighbours that are not duplicates

- **P2760R0 itself** proposes nothing; it is the plan that lists the work (§1).
- Other P2760 items being actively papered by their own authors (e.g. the take/drop-last family, `slice`, `scan`, `adjacent_filter`, `split_when`, `cache_latest`) do not touch trim.
- String-specific trimming discussions (e.g. around `std::string` ergonomics) would not subsume a generic range adaptor, and in any case the index check above covers papered activity under any title.

### 3.3 Residual risks, stated honestly

1. The **2026-10 pre-Búzios mailing** (deadline Fri 23 Oct 2026, 15:00 UTC) had not been published when the index check ran; a competing paper could in principle appear in it. Re-run the §3.1 check against the 2026-10 mailing as soon as it posts.
2. The check covers *papers*. An unpapered std-proposals thread or a draft in someone's public repo would not appear; the mitigation is the float post itself (see `../paper/float-post.txt`), which surfaces the work publicly before a P-number is requested.
3. **Speed risk is real:** the ranges plan is being claimed item by item — Hewill Kang alone holds a string of P2760-derived papers (per the 13 Oct audit in `~/workspace/cpp-proposals/idea-scan/`: P4291 `unique`, P3806 `cycle`, P4294 `take_last`/`drop_last`, P3216 `slice`, P3351 `scan`, P3741 set operations, P3220 `take_before`, P3411 `any_view`). Nothing stops the same happening to `trim` at the next mailing. This is the reason the pack prioritises floating the idea promptly over polishing the wording.

## 4. Design decisions

Each decision: options, evidence, recommendation (which the draft paper and the reference implementation both follow).

### 4.1 Name and shape: the predicate form is the core; `trim(value)` ships alongside

**Decision (as of 13 Oct 2026).** The predicate adaptor is named **`views::trim_if`**, and a value-based companion **`views::trim(value)`** (trims elements equal to `value` from both ends) ships with it, defined in terms of `trim_if`. *(Superseded on the name only, 2026-10-16: the predicate form is now `views::trim_while` — see the note at the top of this document and §V.2 of the draft paper. The two-form split itself stands.)*

**Why not range-v3's name for the predicate form?** range-v3 calls the predicate form `views::trim`. Standard-library naming, however, has a consistent `_if` convention for exactly this split — `remove`/`remove_if`, `replace`/`replace_if`, `count_if`, and the plan's own Tier 1 list uses the `remove`/`remove_if` and `replace`/`replace_if` pairs. A WG21 `views::trim(pred)` would collide with that convention and read as the value form. Renaming relative to range-v3 is a real cost (muscle memory, search results), so the paper flags it as an explicit SG9 question rather than assuming it — but the recommendation is to follow the std convention. Note the plan entry itself just says `trim` (the range-v3 name); either naming is arguably "the plan item", which is precisely why SG9 should steer it early (float post, question 1).

**Why ship the value form at all?** Because the single most common trimming jobs are value-shaped: leading/trailing zeroes, null bytes, a sentinel, spaces in a `char` range. `trim(0)`, `trim('\0')`, `trim(' ')` are each clearer than the equivalent lambda, and the pair pattern is already how the library presents `remove`. Cost: one small forwarding definition. A third conceivable form — `trim` taking a *set* of values (Python's `strip(chars)`, C#'s `Trim(params char[])`) — is **deferred**: it raises lookup questions (linear scan? sorted? hashed? what does "set" even mean generically?) that the predicate form already answers by hand (`trim_while([&](char c){ return chars.contains(c); })`, as the predicate form is now named), and it can be added later without breaking anything. The float post asks SG9 whether it wants the set form in R0 anyway.

**Predicate direction.** `pred(e) == true` ⇒ trim `e`. Consistent with `drop_while` (drop while true), `remove_if` (remove if true), Boost's predicate trim (`trim_if` there — a different library's name, not this proposal's), and range-v3. The alternative reading of the name ("keep-if") is the `filter` convention and would be a trap; the paper states the direction in its abstract to pre-empt the misreading.

### 4.2 Range requirements: bidirectional + common; forward-only is rejected, not degraded

**Decision.** `trim_view` requires the base to model **`bidirectional_range` and `common_range`**, exactly as range-v3 does. Applying the adaptor to a forward-only (or non-common) range is a **compile-time constraint failure**, not a silent leading-edge-only trim.

**Why not degrade?** A `trim` that quietly trims only the front of a forward range is a correctness trap wearing a convenience's clothes: the user's data keeps its trailing junk and no diagnostic fires. The honest tools for a forward range already exist — `views::drop_while(pred)` *is* leading-edge trim, and it says so in its name. Nor is there a lazy alternative for the trailing edge of a forward range: finding the last non-matching element of a non-bidirectional range requires either buffering the whole range (a different, owning adaptor with different complexity and iterator guarantees) or a second pass over a re-readable range with O(n) iterator storage — neither is *this* adaptor.

**Why common_range as well?** The trailing scan must start from an *iterator* to the end and walk backwards; a sentinel that is not an iterator cannot be decremented. (range-v3 requires `common_range` for the same reason.) Mitigation for non-common ranges is the existing one: `views::common` first. Relaxing this in a future revision (e.g. by internally common-ising) is noted as an open question in the paper, not silently assumed.

### 4.3 Mechanism: cached bounds, base iterators

**Decision.** The view stores the base view and the predicate; `begin()` is the first position whose element does not satisfy `pred`; `end()` is one past the last such position; both are computed by a single scan (forward, resp. backward stopping at `begin()`) and cached. The view's iterator and sentinel types are the base's iterator type, so the view **always models `common_range`** and preserves the base's `bidirectional_range` / `random_access_range` / `contiguous_range` properties — the same profile range-v3's test asserts.

**Complexity.** View construction is O(1) (no scan). First `begin()` is O(k) in the trimmed prefix length; first `end()` is O(k′) in the trimmed suffix length; subsequent calls are O(1) via the cache (non-const path). Iteration over the kept elements is exactly the base's. Nothing is copied and no element is visited more than the scans require.

**Const iteration.** As with `drop_while_view`, the const `begin()`/`end()` recompute the bounds rather than reading the cache (the cache is only usable on the non-const path; `iterator_t<const V>` may also differ). This is a known, accepted wart of scanner-style adaptors.

### 4.4 sized_range: follow `drop_while_view`, not range-v3

**Decision.** `trim_view` models `sized_range` **exactly when the base's iterator/sentinel pair models `sized_sentinel_for`** (in practice: random-access and contiguous bases), with `ranges::size` yielding the exact trimmed size via `view_interface::size()` (trimmed end − trimmed begin).

**Evidence.** range-v3 disables sized_range for `trim_view` — but the standard's own `drop_while_view`, whose iterators are likewise the base's own iterators (the closest structural analogue in the standard), **does** model `sized_range` over a `vector` base in libstdc++, because `view_interface::size()` is available and correct there; `take_while_view` does not, only because its sentinel is a distinct type. Verified empirically on 13 Oct 2026 (GCC 13.3.0 / libstdc++): `sized_range<decltype(vector | views::drop_while(p))> == true` and `ranges::size` returns the correct post-drop count.

**Implementation honesty note.** An early draft of the reference implementation tried to imitate range-v3 by specialising `std::ranges::disable_sized_range<trim_view<…>> = true`. The tests showed `sized_range` was *still* satisfied, because `ranges::size` finds the `size()` inherited from `view_interface` — the specialisation does not suppress it in libstdc++. The specialisation was removed: keeping a no-op opt-out would have been dishonest, and the `drop_while_view` precedent says the behaviour is correct anyway. One caveat is recorded in the paper: `size()` on a const view recomputes the bounds (a linear scan), so it does not have the amortised-constant-time character `sized_range` ideally promises; `drop_while_view`'s const `begin()` has the identical property. If SG9 prefers range-v3's stricter answer, the mechanism is to give the view a distinct sentinel type — a bigger design change that the paper lists as an alternative considered.

### 4.5 borrowed_range, common_range propagation

**Decision.** `enable_borrowed_range<trim_view<V, Pred>> = enable_borrowed_range<V>`: if the base is borrowed (e.g. a `span`, or any lvalue via `ref_view`), the trim view is borrowed, because it owns nothing but iterators into the base. The view itself is always a `common_range` (§4.3). Both match range-v3.

### 4.6 No projection in R0

range-v3's `trim` accepts an optional projection, composed with the predicate. The standard's one-sided analogues — `drop_while`, `take_while`, `filter` — do **not** take projections (projections in the standard live on algorithms and a few adaptors like `transform`). **Decision:** no projection in R0, for consistency with the one-sided family; users project first with `views::transform` if needed. Listed as an open question should SG9 disagree.

### 4.7 constexpr

**Decision.** Everything is `constexpr` to the same extent as the base range: constructors, `begin()`/`end()` (both const and non-const), and the closure application. A trim over a `std::array` is usable in `static_assert` today in the reference implementation on both compilers tested (see `../impl/verification-transcript-2026-10-16.txt`), matching the standard's general rule that range adaptors are constexpr-usable where their inputs are.

### 4.8 Feature-test macro

**Decision.** Propose a new macro **`__cpp_lib_ranges_trim`**, value `2026XXL` (placeholder — the year-month of adoption, to be fixed at adoption), in `<ranges>` (and `<version>`). Recent ranges additions have generally received their own macros, but whether a new macro or a bump of `__cpp_lib_ranges` is preferred is an SD-6/SG10 matter; the paper proposes the new macro and flags the choice explicitly rather than presuming it.

## 5. Sources (checked 13 Oct 2026)

- P2760R0, *A Plan for C++26 Ranges* — <https://wg21.link/p2760r0> (§2 table row for `trim`; §6.2 Tier 2 list).
- range-v3 `views::trim` source — <https://github.com/ericniebler/range-v3/blob/master/include/range/v3/view/trim.hpp>
- range-v3 `views::trim` tests — <https://github.com/ericniebler/range-v3/blob/master/test/view/trim.cpp>
- WG21 papers indexes, 2025 and 2026 (full text searched for "trim", 13 Oct 2026; positive controls P4355, P4294) — via <https://wg21.link> index pages.
- isocpp.org committee page (SG9: Chair Jonathan Müller (think-cell), Vice-Chair Zach Laine; LEWG Chair Inbal Levi; officers as of January 2026) — <https://isocpp.org/std/the-committee>
- `drop_while_view` sized_range behaviour: empirical check against GCC 13.3.0 / libstdc++ in this workspace (program and output in the source pack's
  `impl/verification-transcript-2026-10-13.txt`, not copied here).
- Boost.StringAlgo trim/trim_if, Rust `str::trim`/`trim_matches`, Python `str.strip`, Kotlin `trim(predicate)`, C# `String.Trim`, Java `String.strip` — library/language documentation, standard references for §2.2's table (general-knowledge citations, not re-fetched page-by-page; the load-bearing prior art for semantics is range-v3, verified from source above).
