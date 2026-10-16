---
title: "Add std::views::drop_last_while and std::views::take_last_while"
document: DxxxxR0
date: 2026-10-15
audience: SG9 Ranges, LEWG
reply-to: "Anish Aggarwal <anishagg17@gmail.com>"
---

# Add `std::views::drop_last_while` and `std::views::take_last_while`

| | |
|---|---|
| Document number: | DxxxxR0 *(draft; no P number has been requested yet)* |
| Date: | 2026-10-15 |
| Project: | ISO JTC1/SC22/WG21: Programming Language C++ |
| Audience: | SG9 Ranges, Library Evolution Working Group (LEWG) |
| Reply-to: | Anish Aggarwal \<anishagg17@gmail.com\> |
| Target: | C++29 |

## Abstract

[P2760R1], *A Plan for C++26 Ranges*, lists `views::drop_last_while`
and `views::take_last_while` as **Tier 1** range adaptors, each
annotated "(not in range-v3)". Neither has been papered. This paper
proposes both: `drop_last_while(pred)` is a range adaptor closure
that yields the elements of a bidirectional range with the maximal
trailing run of elements satisfying `pred` removed, and
`take_last_while(pred)` yields that trailing run, in the
range's original order. They are the back-end mirrors of
`views::drop_while` and `views::take_while`, and they complete the
take/drop family: the counted back-end pair, `views::drop_last` /
`views::take_last`, is already proposed by [P4294R0].

```cpp
std::vector<int> v = {1, 2, 3, 0, 0};
auto d = v | std::views::drop_last_while(is_zero); // {1, 2, 3}
auto t = v | std::views::take_last_while(is_zero); // {0, 0}
std::string s = "hello  ";
auto u = s | std::views::drop_last_while(is_space); // "hello"
```

The operation is a named standard facility in other ecosystems
(Kotlin's standard library has shipped the generic pair since 1.0,
Haskell's `base` the drop side since 2012, Lodash the pair since
3.0.0) and is measurably used in production code (§III.2). In C++
it is today written by hand, in stereotyped and error-prone forms,
including inside major libraries (§III.3). The adaptors proposed
here require a bidirectional, common base range; the boundary is
found by a single backward scan and cached, so both views' iterators
are the base range's own iterators, the result is always a common
range, and the base's iterator category is preserved. Composing the
operation from `views::reverse` and `views::drop_while` /
`views::take_while` is possible but loses those properties
(§III.1). Two design questions are put to SG9 as explicit polls
(§VIII): whether this pair should instead be merged into the
author's `views::trim` proposal, and whether a forward-range mode
should exist.

## I. Table of contents

- Abstract
- I. Table of contents
- II. Introduction
- III. Motivation and scope
- IV. Impact on the standard
- V. Design decisions
- VI. Technical specification (draft wording)
- VII. Implementation experience
- VIII. Polls requested
- IX. Acknowledgements
- X. References
- Appendix A: Alternatives considered
- Appendix B: Open questions
- Revision history

## II. Introduction

[P2760R1] is SG9's prioritised plan for the ranges library. In its
table of range adaptors, `drop_last_while` and `take_last_while`
appear at **Tier 1**, the plan's first-priority tier, each marked
"(not in range-v3)": unlike most plan items, there is no range-v3
semantics to port, so the design work is open. The surrounding
family has moved first: [P4294R0] proposes the counted back-end
pair (`drop_last`/`take_last`), and the front-end predicate pair
(`drop_while`/`take_while`) has been in the standard since C++20.
The back-end predicate pair is the remaining quadrant.

This proposal was floated on std-proposals on 14 October 2026.
Hewill Kang replied that, as far as he knows, no one in SG9 is
working on it, and asked that the paper justify why these views
belong in the standard library, backed by research into use cases:
how frequently these utilities are used in real code, and what
other programming languages provide. Section III is that research,
in condensed form; the full study accompanies this paper.

## III. Motivation and scope

### III.1 The gap

Given a bidirectional range and a predicate, obtaining the subrange
that remains after removing the longest matching *suffix* (or
obtaining that suffix itself) currently looks like:

```cpp
auto last = std::ranges::end(rng);
auto first = std::ranges::begin(rng);
while (last != first) {
    auto prev = std::ranges::prev(last);
    if (!pred(*prev)) break;
    last = prev;
}
auto kept = std::ranges::subrange(first, last);   // drop_last_while
auto run  = std::ranges::subrange(last, std::ranges::end(rng)); // take_last_while
```

This is short but easy to get subtly wrong (the `last != first`
guard: without it, an all-matching range reads before its begin),
and it has to be rewritten at every call site because no adaptor
captures it:

- `views::drop_while(pred)` / `views::take_while(pred)` operate
  from the front only.
- Composing from the back (`rng | views::reverse |
  views::drop_while(pred) | views::reverse`) works, and is lazy,
  but the result's iterators are wrapper iterators, not the base's
  own. A position in the original range can be dug out of the
  composition only by unwrapping each `reverse_iterator`'s
  `.base()`, with the off-by-one adjustment that operation needs
  at every layer; there is no direct way to form a `subrange` or
  `string_view` of the kept part or to read off offsets into the
  original range, and the result is not a `contiguous_range` even
  when `rng` is one. For the take side the composition is worse: a single
  `views::reverse | views::take_while(pred)` yields the trailing
  run **in reverse order**, terminated by a sentinel rather than an
  iterator, so recovering the original order needs the second
  reverse and its full wrapper stack.
- `std::ranges::find_last_if_not(rng, pred)` (C++23) comes
  closest: it returns the last element for which `pred` is false,
  so the boundary is one call away. The caller must still advance
  the result by one (and handle the all-matching case, in which
  the function returns the end iterator), form both subranges by
  hand at every call site, and what results is an iterator, not a
  lazy view that composes in a pipeline.
- `views::drop_last(n)` / `views::take_last(n)` ([P4294R0]) are
  count-based; they cannot express "while the predicate holds".

### III.2 The operation is standard elsewhere, and used

The strongest evidence that this is demand rather than symmetry is
that other ecosystems adopted the operation independently, under
stable names, and kept it:

| Facility | Since | Form |
|---|---|---|
| Kotlin stdlib `dropLastWhile` / `takeLastWhile` | Kotlin 1.0 | generic over collections, arrays, `CharSequence` (eager) |
| Haskell `base`, `Data.List.dropWhileEnd` | base 4.5.0.0 (2012) | lists; the take side is in `text` / `bytestring` / `Data.List.Extra` |
| Lodash `_.dropRightWhile` / `_.takeRightWhile` | 3.0.0 | arrays/objects (eager) |
| Deno standard library `@std/collections` | n/a | generic pair |
| Apple swift-algorithms `trimmingSuffix(while:)` / `suffix(while:)` | n/a | `BidirectionalCollection` methods returning non-owning subsequences |
| Ramda, Remeda, es-toolkit, Go's samber/lo | various | ecosystem libraries |

Languages without a generic facility (Python, Java, C#, Ruby,
Scala, Elixir, Clojure) consistently strand users at string-specific
trims (`rstrip`, `TrimEnd`, `rtrim`) or at reverse compositions;
Rust has the string form (`str::trim_end_matches`); a generic
slice version was accepted by the Rust libs-api team in September
2024 (rust-lang/libs-team#414) but is not yet in a stable release. Two ecosystems show documented,
recurring requests for the missing piece (a long-standing
Scala feature issue; a Swift Forums thread in which hand-written
`dropLast(while:)` implementations contained off-by-one errors
corrected in-thread).

**Measured usage.** Direct-API usage was counted on two public
code indexes on 15 October 2026. The unit is *files containing at
least one match*; counts include tests, documentation and vendored
copies and are not deduplicated, so they support "recurring,
cross-ecosystem, named demand", not a precise market size:

| Spelling | grep.app | Sourcegraph |
|---|---|---|
| `dropLastWhile` (Kotlin stdlib / Ramda) | 1,744 files | 4,738+ files |
| `takeLastWhile` | 623 files | 3,098 files |
| `dropWhileEnd` (Haskell) | 626 files | 740 files |
| `takeWhileEnd` | 152 files | 218+ files |
| `dropRightWhile` (Lodash) | ≈3,138 files | 13,396+ files |
| `takeRightWhile` | ≈3,024 files | 13,852 files |

The two columns come from two separate counting passes, one per
index, both run on 15 October 2026 and each described in the
accompanying research. In the grep.app pass, Sourcegraph returned
a bot-verification page; in the Sourcegraph pass, the grep.app API
was blocked (HTTP 429). Neither pass could cross-check the other,
and the indexes have different universes and counting methods
(several Sourcegraph figures are lower bounds, the result limit
having been reached), so the columns are not a same-conditions
comparison. The Lodash-spelling counts on both indexes are
inflated by vendored copies of Lodash itself and are quoted as
approximate. An exact GitHub-wide count could not be obtained
(GitHub Code Search requires sign-in) and is not claimed.

Both operations are used in thousands of files across at least
four language ecosystems. The drop side is used more than the
take side, by a factor that depends on the index and the spelling
pair: on grep.app, ≈2.8× for `dropLastWhile`/`takeLastWhile`
(1,744 vs 623 files) and ≈4.1× for `dropWhileEnd`/`takeWhileEnd`
(626 vs 152); on Sourcegraph, ≈1.5× for
`dropLastWhile`/`takeLastWhile` (4,738 vs 3,098 files) and ≈3.4×
for `dropWhileEnd`/`takeWhileEnd` (740 vs 218). The Lodash pair
is near parity on both indexes, which the vendoring distortion
makes hard to read. The `dropLastWhile` spelling is not one
language's either: Sourcegraph's files for it split almost evenly
between Kotlin (2,192 files) and JavaScript (2,195 files,
Ramda-style usage of the same name), and its `takeLastWhile`
files are mostly JavaScript (2,122 of 3,098), with 456 in
Kotlin. Section V.1 explains why the pair is proposed together
despite the drop side's lead.

### III.3 Concrete use cases

The recurring patterns are ordinary ones; each of the
following is a named, linkable instance from production code or a
specification (full catalogue in the accompanying research):

- **Discarding trailing empty fields after splitting.** Kotlin's
  `split(...).dropLastWhile { it.isEmpty() }` is the single most
  repeated concrete pattern in the Kotlin results (URL and cookie
  parsing in Pocket Casts, EhViewer, and many others). Java's
  `String.split` makes the same choice a platform default: with
  limit 0 it discards trailing empty fields. That is the operation
  acting on a sequence of fields, implicitly; the silent-data-loss
  surprises argue for an explicit named form.
- **Stripping a trailing run of non-text elements.** Python's
  `packaging` library strips trailing zero *integer components*
  from a version's release tuple before comparison, by an explicit
  backward loop in current code, and, in an earlier vendored
  version, by literally reversing, applying a front drop-while,
  and reversing back. The run here is over a tuple of integers,
  not characters.
- **Paths.** GHC's filepath library strips trailing path
  separators with `dropWhileEnd isPathSeparator` (and trailing
  spaces from file names likewise).
- **Protocol text.** Haskell's WAI builds its whitespace trim on
  `dropWhileEnd`; the WHATWG URL standard and HTTP's [RFC9110]
  both mandate trailing-whitespace removal in parsing.
- **Suffix extraction (the take side).** Pandoc extracts the part
  of a tag after the last `':'`, the last line of a text, and the
  part of a name after the last `'.'` with `takeWhileEnd`; Wasp
  extracts a port from `docker` output the same way. These are
  `take_last_while`: the suffix *is* a maximal trailing run
  defined by a predicate.
- **Numeric formatting and data.** NumPy's `polytrim` removes
  trailing polynomial coefficients that are zero *or below a
  tolerance* (a predicate, not a fixed value); LibreOffice exposes
  "erase trailing decimal zeros" as a named formatting option;
  Sentry's web UI drops trailing undefined column widths with
  `dropRightWhile` before layout.
- **Inside C++ libraries today.** Abseil's
  `StripTrailingAsciiWhitespace` is implemented as a
  reverse-iterator `find_if_not`, the manual idiom of §III.1
  productised for one predicate; LLVM's `StringRef::rtrim` is a
  find-last-not-of plus a view return, effectively this adaptor
  hand-built for `char`. C++'s own string-specific mechanism,
  `find_last_not_of`, appears in 7,928 files on grep.app (25,071+
  C++ files on Sourcegraph), the broadest available proxy for C++
  demand for an operation the language does not otherwise offer.

No primary-source *binary-protocol* example (e.g. stripping
trailing padding from a fixed-size record) was captured in the
research, so that domain is not claimed here.

### III.4 Scope

In scope: two view classes, `drop_last_while_view` and
`take_last_while_view`, exposed through two range adaptor objects
in `<ranges>`, following the design of the standard's one-sided
scanner adaptors (`drop_while_view` above all).

Out of scope: algorithms that mutate or return a `subrange`
(a plausible future companion, not proposed here); a value-based
form (see §V.1); support for input or forward-only ranges (see
§V.3, and Poll 2); string-specific member functions (a separate
concern).

## IV. Impact on the standard

The change is a pure library addition to `<ranges>`:

- two new class templates, `std::ranges::drop_last_while_view`
  and `std::ranges::take_last_while_view`, in new subclauses of
  [range.adaptors];
- two new range adaptor objects, `std::ranges::views::drop_last_while`
  and `std::ranges::views::take_last_while`;
- specialisations of `std::ranges::enable_borrowed_range` for both
  view types;
- a feature-test macro (proposed; see §V.7).

No existing wording is modified, no core-language change is
required, and nothing is deprecated. The addition is implementable
in C++20 terms on top of the existing ranges machinery; a reference
implementation accompanies this paper (§VII).

## V. Design decisions

### V.1 The pair, together; predicate direction; no value form

Throughout, `pred(e) == true` means *e belongs to the trailing
run*: `drop_last_while` drops it, `take_last_while` keeps it.
This matches `drop_while` / `take_while` (the predicate selects
what the adaptor acts on), Kotlin, Haskell and Lodash. The
direction is stated here and in the wording explicitly; it is the
single easiest thing to misread about these names.

The pair is proposed in one paper because the two views are one
operation seen from two sides: they share one boundary
computation, and each is the other's complement (drop ++ take is
the identity on the base). The usage asymmetry of §III.2 is real (the drop
side leads by ≈1.5× to ≈4.1×, depending on index and spelling
pair); the take side is justified on its own evidence (suffix extraction
in parsers and tooling, §III.3) and by the precedent of the
counted pair, which [P4294R0] also proposes together. Splitting
the paper so that only the drop side advances is available to SG9
as an outcome of Poll 3's discussion, but is not the
recommendation: a boundary computed for dropping is the same
boundary the take side needs, and standardising one without the
other would leave the partition incomplete.

No value-based form (an equality-predicate companion, as
`remove` accompanies `remove_if`) is proposed in R0:
`drop_while`/`take_while` have none, and `drop_last_while` with a
short lambda is not burdensome. It can be added later without
disturbing these names.

### V.2 Names

The names are P2760R1's own, and they complete the library's
existing grid without invention: front/back (`drop_while` vs
`drop_last_while`) crossed with count/predicate (`drop_last` vs
`drop_last_while`). The Haskell spelling (`dropWhileEnd`) and the
Lodash spelling (`dropRightWhile`) were considered and rejected:
`_last` is the suffix the standard's own counted pair ([P4294R0])
already uses for this end of the range, and consistency within
the family outweighs either external precedent.

### V.3 Requirements: bidirectional and common; no degraded mode

Both views require the base range to model `bidirectional_range`
and `common_range`, and `pred` to model
`indirect_unary_predicate<const Pred, iterator_t<V>>`; the
predicate is always invoked as const.

- *Bidirectional* is what makes the operation cheap and the
  evaluation guarantee of §V.5 possible: the boundary is found by
  one backward scan whose cost is proportional to the trailing run
  (plus one failing application), not to the range. For a
  forward-only range there is no lazy version of this at all: the
  boundary can only be found by traversing the whole range and
  remembering the last position where the predicate failed. The
  result is either a view whose `begin()` rescans the range on
  every access, or a buffering, owning adaptor with different
  iterator guarantees. Neither is this adaptor. Apple's swift-algorithms
  reached the same conclusion independently: its
  `trimmingSuffix(while:)` is `BidirectionalCollection`-only, and
  its guide documents the refusal of a silently inefficient
  forward fallback as a deliberate choice. Applying the adaptors
  to a forward-only range is therefore a compile-time constraint
  failure (demonstrated in the reference implementation's tests).
  Whether SG9 wants a forward mode anyway is Poll 2.
- *Common* is needed because the backward scan must start from an
  iterator to the end; a sentinel cannot be decremented. The
  existing remedy for non-common ranges, piping through
  `views::common` first, applies unchanged. Relaxing this (by
  common-ising internally) is an open question (Appendix B, Q2),
  not assumed.

### V.4 Mechanism and concept profile

Each view stores the base view and the predicate, the latter in
a *movable-box*, the exposition-only wrapper `drop_while_view`
uses, so that a predicate which is constructible but not
assignable (a capturing lambda, the everyday case) still yields a
view that models `view` and can be piped into further adaptors.
One boundary position is shared by both views: the first position
of the trailing run, equivalently one past the last element not
satisfying `pred`. `drop_last_while_view::begin()` is the base's
begin and its `end()` is the boundary; `take_last_while_view` is
the mirror. The boundary is computed at most once and cached (the
standard scanner-adaptor pattern, cf. `drop_while_view`'s cached
begin; the cache is a *non-propagating-cache*, for the reasons
given in the wording's note).

Because the bounds *are* base iterators, no iterator wrapper
class is needed:

| Property | Behaviour |
|---|---|
| `view` | yes |
| `common_range` | always (begin and end have the base iterator type) |
| `bidirectional_range` | iff the base is bidirectional (always, by the constraint) |
| `random_access_range` | iff the base is random-access |
| `contiguous_range` | iff the base is contiguous |
| `borrowed_range` | iff the base is borrowed (`enable_borrowed_range` propagates) |
| `sized_range` | iff the base's iterator/sentinel pair is a `sized_sentinel` pair (see §V.6) |

Construction of either view is O(1). The first `begin()`/`end()`
that needs the boundary costs one scan of the trimmed suffix;
afterwards both bounds are O(1). Kept elements are visited
as in the base range; nothing is copied, and the take side yields
its elements in the base's original order.

There are deliberately no const `begin()`/`end()`. A const
overload could only recompute the boundary on every call, and
`begin()` for a range is required to be amortized constant time;
a per-call rescan of the trailing run would break that
requirement. `drop_while_view` makes the same choice: it has no
const `begin()` at all. A const view object is therefore not
itself a range.

### V.5 Predicate evaluation: order and count

The scan's evaluation behaviour is part of this proposal's
specified semantics, not an implementation detail: the predicate
is applied **starting from the last element, moving backwards**,
and evaluation stops at the first element for which the predicate
is false. Two observable consequences:

- **Count.** The predicate is applied at most (trailing run
  length + 1) times to compute the boundary ((run length) times
  when the whole range matches) and, the boundary being cached,
  at most once per view in total. Elements before the
  boundary are never evaluated. (The forward-scan alternative
  would evaluate *every* element; the difference is observable
  and is a further reason §V.3's bidirectional requirement is
  semantic, not merely mechanical.)
- **Order.** Evaluation is in reverse element order, the
  opposite of `drop_while`'s front-to-back order. For pure
  predicates this is invisible; for stateful, counting, or
  expensive predicates it is a real, documented difference, and
  users porting a predicate from `drop_while` need it stated
  rather than discovered. The reference implementation's tests
  record the application sequence and assert both the order and
  the count.

Whether the *wording* should pin the order strictly, or specify
only the boundary's value and the stopping condition (the latitude
scanner-adaptor wording usually allows), is flagged for LWG in
Appendix B, Q3; the design intent is that the count bound and the
short-circuit are normative, since both are observable through
the predicate itself.

### V.6 sized_range: the `drop_while_view` precedent

Both views model `sized_range` exactly when the base's
iterator/sentinel pair models `sized_sentinel_for`, and
`ranges::size` then returns the exact post-adaptation size
(boundary − begin, resp. end − boundary) via
`view_interface::size()`. This follows the standard's own
`drop_while_view`, whose iterators are likewise the base's and
which models `sized_range` over e.g. a `vector` base for exactly
this reason; `take_while_view` fails to model it only because its
sentinel is a distinct type, which is not our situation: both
bounds here are iterators. Computing `size()` requires the
boundary, so the first `size()` call pays for the scan; with the
boundary cached, later calls are O(1), the same profile as
`drop_while_view`'s `size()`.

### V.7 constexpr and the feature-test macro

All constructors and member functions of both views, and both
adaptor objects' call operators, are `constexpr`, so the adaptors
are usable in constant expressions wherever the base range is
(demonstrated in the reference implementation over `std::array`).

Feature-test macro: this paper proposes a new macro
**`__cpp_lib_ranges_drop_last_while`**, value `2026XXL`, defined
by `<ranges>` and `<version>`, covering both adaptors. Whether a
per-feature macro or a bump of `__cpp_lib_ranges` is preferred is
an SD-6 matter; the choice and the value's digits are placeholders
for SG10/LWG to settle, and are flagged as such in the wording.

## VI. Technical specification (draft wording)

*Editorial note: the following is a **wording sketch** in the style
of [range.adaptors], prepared for design review. It has not been
reviewed by LWG, paragraph numbering is relative, and bracketed
notes mark the places where LWG input is expected. It is written
against the C++26 working draft's structure for [range.adaptors].
The two-argument constructors are `explicit`, following
[P2711R1].*

### VI.1 Header `<ranges>` synopsis

Add to namespace `std::ranges`, in the range adaptors synopsis:

```cpp
// [range.drop.last.while], drop last while view
template<view V, class Pred>
  requires bidirectional_range<V> && common_range<V> && is_object_v<Pred> &&
           indirect_unary_predicate<const Pred, iterator_t<V>>
class drop_last_while_view;

// [range.take.last.while], take last while view
template<view V, class Pred>
  requires bidirectional_range<V> && common_range<V> && is_object_v<Pred> &&
           indirect_unary_predicate<const Pred, iterator_t<V>>
class take_last_while_view;

namespace views {
  inline constexpr /* unspecified */ drop_last_while = /* unspecified */;
  inline constexpr /* unspecified */ take_last_while = /* unspecified */;
}
```

and the borrowed-range specialisations:

```cpp
template<class V, class Pred>
  inline constexpr bool enable_borrowed_range<drop_last_while_view<V, Pred>>
    = enable_borrowed_range<V>;
template<class V, class Pred>
  inline constexpr bool enable_borrowed_range<take_last_while_view<V, Pred>>
    = enable_borrowed_range<V>;
```

### VI.2 New subclause [range.drop.last.while]

> **26.7.x Drop last while view [range.drop.last.while]**
>
> **26.7.x.1 Overview [range.drop.last.while.overview]**
>
> (1) `drop_last_while_view` takes a bidirectional common range and
> a predicate, and produces a view of the elements of the range
> with the maximal trailing run of elements satisfying the
> predicate removed.
>
> (2) The name `views::drop_last_while` denotes a range adaptor
> object ([range.adaptor.object]). Given subexpressions `E` and
> `P`, the expression `views::drop_last_while(E, P)` is
> expression-equivalent to `drop_last_while_view(E, P)`.
>
> *Example:*
> ```cpp
> std::vector<int> v = {1, 2, 3, 0, 0};
> auto d = v | views::drop_last_while(is_zero); // d is a view of {1, 2, 3}
> ```
>
> **26.7.x.2 Class template `drop_last_while_view` [range.drop.last.while.view]**
>
> ```cpp
> namespace std::ranges {
>   template<view V, class Pred>
>     requires bidirectional_range<V> && common_range<V> && is_object_v<Pred> &&
>              indirect_unary_predicate<const Pred, iterator_t<V>>
>   class drop_last_while_view
>     : public view_interface<drop_last_while_view<V, Pred>> {
>   private:
>     V base_ = V();                                    // exposition only
>     movable-box<Pred> pred_;                          // exposition only
>     non-propagating-cache<iterator_t<V>> end_;        // exposition only
>   public:
>     drop_last_while_view()
>       requires default_initializable<V> && default_initializable<Pred>
>       = default;
>     constexpr explicit drop_last_while_view(V base, Pred pred);
>
>     constexpr V base() const& requires copy_constructible<V> { return base_; }
>     constexpr V base() && { return std::move(base_); }
>     constexpr const Pred& pred() const noexcept { return *pred_; }
>
>     constexpr iterator_t<V> begin();
>     constexpr iterator_t<V> end();
>   };
>
>   template<class R, class Pred>
>     drop_last_while_view(R&&, Pred) -> drop_last_while_view<views::all_t<R>, Pred>;
> }
> ```
>
> `constexpr explicit drop_last_while_view(V base, Pred pred);`
> (3) *Effects:* Initializes `base_` with `std::move(base)` and
> `pred_` with `std::move(pred)`.
>
> `constexpr iterator_t<V> begin();`
> (4) *Returns:* `ranges::begin(base_)`.
>
> `constexpr iterator_t<V> end();`
> (5) *Preconditions:* `pred_.has_value()` is `true`.
> (6) *Effects:* If `end_` does not contain a value, initializes it
> as follows: let `last` be `ranges::end(base_)`; while `last` is
> not equal to `ranges::begin(base_)`, let `p` be an iterator such
> that `++p == last`; if `bool(std::invoke(*pred_, *p))` is
> `false`, stop; otherwise set `last` to `p`. Initializes `end_`
> with `last`. The predicate is applied to the elements of
> `base_`, starting from the last element and proceeding
> backwards, at most the number of elements in the trailing run
> plus one times. *[Note: whether the evaluation order itself,
> beyond the count and the stopping condition, should be normative
> is for LWG; see §V.5. — end note]*
> (7) *Returns:* `*end_`.

### VI.3 New subclause [range.take.last.while]

> **26.7.y Take last while view [range.take.last.while]**
>
> **26.7.y.1 Overview [range.take.last.while.overview]**
>
> (1) `take_last_while_view` takes a bidirectional common range and
> a predicate, and produces a view of the maximal trailing run of
> elements of the range satisfying the predicate, in the order in
> which they appear in the range.
>
> (2) The name `views::take_last_while` denotes a range adaptor
> object ([range.adaptor.object]). Given subexpressions `E` and
> `P`, the expression `views::take_last_while(E, P)` is
> expression-equivalent to `take_last_while_view(E, P)`.
>
> *Example:*
> ```cpp
> std::vector<int> v = {1, 2, 3, 0, 0};
> auto t = v | views::take_last_while(is_zero); // t is a view of {0, 0}
> ```
>
> **26.7.y.2 Class template `take_last_while_view` [range.take.last.while.view]**
>
> ```cpp
> namespace std::ranges {
>   template<view V, class Pred>
>     requires bidirectional_range<V> && common_range<V> && is_object_v<Pred> &&
>              indirect_unary_predicate<const Pred, iterator_t<V>>
>   class take_last_while_view
>     : public view_interface<take_last_while_view<V, Pred>> {
>   private:
>     V base_ = V();                                    // exposition only
>     movable-box<Pred> pred_;                          // exposition only
>     non-propagating-cache<iterator_t<V>> begin_;      // exposition only
>   public:
>     take_last_while_view()
>       requires default_initializable<V> && default_initializable<Pred>
>       = default;
>     constexpr explicit take_last_while_view(V base, Pred pred);
>
>     constexpr V base() const& requires copy_constructible<V> { return base_; }
>     constexpr V base() && { return std::move(base_); }
>     constexpr const Pred& pred() const noexcept { return *pred_; }
>
>     constexpr iterator_t<V> begin();
>     constexpr iterator_t<V> end();
>   };
>
>   template<class R, class Pred>
>     take_last_while_view(R&&, Pred) -> take_last_while_view<views::all_t<R>, Pred>;
> }
> ```
>
> `constexpr explicit take_last_while_view(V base, Pred pred);`
> (3) *Effects:* Initializes `base_` with `std::move(base)` and
> `pred_` with `std::move(pred)`.
>
> `constexpr iterator_t<V> begin();`
> (4) *Preconditions:* `pred_.has_value()` is `true`.
> (5) *Effects:* If `begin_` does not contain a value, initializes
> it as follows: let `last` be `ranges::end(base_)`; while `last`
> is not equal to `ranges::begin(base_)`, let `p` be an iterator
> such that `++p == last`; if `bool(std::invoke(*pred_, *p))` is
> `false`, stop; otherwise set `last` to `p`. Initializes `begin_`
> with `last`. The predicate-evaluation remarks of
> [range.drop.last.while] (6) apply.
> (6) *Returns:* `*begin_`.
>
> `constexpr iterator_t<V> end();`
> (7) *Returns:* `ranges::end(base_)`.
>
> *[Note (applies to both subclauses): the caches are*
> non-propagating-cache*s, as with `drop_while_view`: copying or
> moving either view does not carry the cached boundary into the
> new view, and moving empties the source's cache as well. A cached
> iterator may point into the source view object itself (e.g. when
> the base is `views::single`, which stores its element inside the
> view), so propagating it would leave the copy (or the
> moved-from original) with an iterator into storage the other
> object owned. Whether the caches should instead be specified
> behaviorally ("begin() may cache its result") is for LWG; the
> observable semantics are identical either way. — end note]*

### VI.4 Feature-test macro

Add to the feature-test macro table ([version.syn], and `<ranges>`):

```cpp
#define __cpp_lib_ranges_drop_last_while 2026XXL // also in <ranges>
```

*[Value and the new-macro-vs-`__cpp_lib_ranges`-bump choice are
placeholders; see §V.7.]*

## VII. Implementation experience

A portable C++20 reference implementation accompanies this paper
(`std_proposal::ranges::drop_last_while_view` and
`take_last_while_view`, header-only, in namespace `std_proposal`
rather than `std`). It implements the design of §V,
including the constraint set of §V.3 (misuse on a forward-only or
non-common range is a substitution failure, checked by
concept-based tests), the cached-boundary mechanism of §V.4, the
predicate-evaluation behaviour of §V.5 (a test records the
application sequence and asserts the back-to-front order and the
run-length-plus-one count, and that the cached boundary is not
rescanned), and borrowed-range propagation.

As of 15 Oct 2026: the test suite compiles warning-free under
`-Wall -Wextra -Werror` and runs to `PASS` in all four
configurations (g++ 13.3.0 and clang++ 18.1.3 (libstdc++), each
at `-std=c++20` and `-std=c++23`) and runs clean under
AddressSanitizer and UndefinedBehaviorSanitizer under both
compilers at `-std=c++20`. The suite covers the boundary
behaviours (empty range; all elements matching; none matching;
partial trailing run with a leading run present; single element),
the partition property (drop ++ take is the base), order
preservation on the take side over a non-palindromic trailing
run, the split-then-drop-empty-fields pattern of §III.3,
composition with `transform` and with `drop_while` (i.e. trim
built from its parts), the concept profile of §V.4 including
borrowed-range propagation and the sized_range profile of §V.6,
concept-based negative tests showing forward-only and non-common
bases are rejected, `static_assert` evaluation over `std::array`,
and regression tests for the non-propagating caches (copy over a
`views::single` base; move over an owning base) that the
sanitizer runs exercise. In C++23 the adaptor closure types
derive from `std::ranges::range_adaptor_closure`, so closures
compose before application; C++20 provides range | closure only.
Exact commands and outputs: `impl/verification-transcript-2026-10-15.txt`
in the accompanying implementation pack.

One point is worth stating plainly, because it is easy to
misread when composing with the prefix adaptors: `pred` selects
the elements of the trailing run, i.e. the elements
`drop_last_while` drops, not the elements it keeps (§V.1). No
performance claims are made: the adaptors' costs are the
base range's plus one bounded scan, and no benchmark programme
(beyond the complexity analysis of §V.4–V.5) has been run.

## VIII. Polls requested

1. **Separate vs merged:** proceed with `drop_last_while` /
   `take_last_while` as this separate paper, rather than merging
   them into the author's `views::trim` proposal (floated the same
   week; trim is, to first order, `drop_while` followed by
   `drop_last_while`)? *(This question was asked on the float post
   and drew no answer; the draft proceeds separately (different
   plan tier, Tier 1 vs Tier 2; different prior-art situation)
   and asks SG9 to decide it on the record.)*
2. **Requirements:** require `bidirectional_range` and
   `common_range`, with no forward-only mode, as in §V.3?
3. **Pair:** forward both views together (§V.1), rather than
   advancing `drop_last_while` alone?
4. **Direction:** forward this design to LEWG for C++29?

## IX. Acknowledgements

Thanks to the authors of [P2760R1] for the plan entry this paper
implements, and to Hewill Kang, whose reply to the float post
asked for the use-case and frequency research of §III, and who
replied that, as far as he knows, no one in SG9 is working on
this. Thanks also to Vinnie Falco for endorsing that request. The use-case research
draws on the public documentation and source of the Kotlin,
Haskell, Lodash, Deno and swift-algorithms projects, and on the
public code indexes cited in §III.2.

## X. References

- [P2760R1] *A Plan for C++26 Ranges.* <https://wg21.link/p2760r1>
- [P4294R0] *`views::drop_last` and `views::take_last`* (Hewill Kang).
- [P2711R1] *Making multi-param constructors of views `explicit`.*
  <https://wg21.link/p2711r1>
- Rust libs-api proposal, slice `trim` methods (accepted September
  2024; not yet stable): <https://github.com/rust-lang/libs-team/issues/414>
- [RFC9110] *HTTP Semantics.* <https://www.rfc-editor.org/rfc/rfc9110>
- Kotlin standard library, `dropLastWhile` / `takeLastWhile`:
  <https://kotlinlang.org/api/core/kotlin-stdlib/kotlin.text/drop-last-while.html>,
  <https://kotlinlang.org/api/core/kotlin-stdlib/kotlin.text/take-last-while.html>
- Haskell `base`, `Data.List.dropWhileEnd`; `text`, `Data.Text.takeWhileEnd`:
  <https://hackage.haskell.org/package/base> (docs)
- Lodash documentation, `_.dropRightWhile` / `_.takeRightWhile`:
  <https://lodash.com/docs/4.18.1>
- Deno standard library, `@std/collections`:
  <https://jsr.io/@std/collections/doc/all_symbols>
- Apple swift-algorithms, Trim and Suffix guides:
  <https://github.com/apple/swift-algorithms/blob/main/Guides/Trim.md>,
  <https://github.com/apple/swift-algorithms/blob/main/Guides/Suffix.md>
- Python `packaging`, `version.py` (`_cmpkey`):
  <https://github.com/pypa/packaging/blob/main/src/packaging/version.py>
- Java SE documentation, `String.split`:
  <https://docs.oracle.com/javase/8/docs/api/java/lang/String.html>
- NumPy, `numpy.polynomial.polynomial.polytrim`:
  <https://numpy.org/doc/stable/reference/generated/numpy.polynomial.polynomial.polytrim.html>
- grep.app code index (counts read 2026-10-15):
  <https://grep.app/search?q=dropLastWhile> and companion queries
  listed in the accompanying research.
- Sourcegraph public code search (counts read 2026-10-15 via the
  unauthenticated GraphQL API).

## Appendix A: Alternatives considered

- **Composition only (status quo):** `reverse | drop_while |
  reverse` and friends. Rejected in §III.1: wrapper iterators,
  loss of contiguity, original-range positions recoverable only by
  unwrapping `.base()` chains, reversed-order output on the take
  side through a single reverse, and a paragraph of explanation
  at every call site. (Production Haskell codebases (Cabal, GHC)
  independently replaced the composed form with hand-written
  implementations, citing speed and readability.)
- **Forward-range support by buffering:** an owning adaptor that
  materialises the range (or its suffix) and therefore has
  different complexity, iterator and lifetime guarantees.
  Rejected for R0 (§V.3); it is a different facility and can be
  proposed on its own if SG9 wants it; Poll 2 tests the appetite.
- **Relaxing `common_range` by common-ising internally:** costs a
  wrapper iterator for every user, including the common-case ones,
  to save an explicit `views::common` at the call site for the
  rest. Rejected in R0; Appendix B, Q2.
- **Distinct sentinel type / never-sized:** rejected for §V.6's
  reasons (the `drop_while_view` precedent and the correctness of
  the inherited `size()`).
- **Haskell/Lodash spellings (`drop_while_end`, `drop_right_while`):**
  rejected in §V.2 for family consistency with `drop_last` /
  `take_last`.
- **One view class with a mode flag instead of two:** the
  front-end family uses distinct classes (`drop_while_view`,
  `take_while_view`); consistency wins.

## Appendix B: Open questions

- Q1. Should a value-based form accompany the pair in a later
  revision (the `remove`/`remove_if` pattern), or is the lambda
  sufficient permanently?
- Q2. Should the `common_range` requirement be relaxed by
  common-ising internally, at the cost of a wrapper iterator?
- Q3. Predicate evaluation in the wording: pin the back-to-front
  order normatively, or specify only the boundary's value, the
  count bound and the stopping condition? (§V.5)
- Q4. Should a projection be supported (the standard's one-sided
  family does not have one)?
- Q5. Feature-test macro: new `__cpp_lib_ranges_drop_last_while`
  vs a bump of `__cpp_lib_ranges` (SG10/SD-6 steer).
- Q6. If SG9 merges this pair into the `views::trim` paper
  (Poll 1), which paper's structure survives, and does trim then
  get specified *as* `drop_while` + `drop_last_while`?

## Revision history

- **DxxxxR0 (2026-10-15):** initial draft for SG9 discussion.
  Circulated as a D-number draft; no P number requested yet.
  Preceded by the std-proposals float post of 2026-10-14 and the
  replies summarised in §II.
