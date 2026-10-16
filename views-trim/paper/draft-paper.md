---
title: "Add std::views::trim and std::views::trim_while"
document: DxxxxR0
date: 2026-10-16
audience: SG9 Ranges, LEWG
reply-to: "Anish Aggarwal <anishagg17@gmail.com>"
---

# Add `std::views::trim` and `std::views::trim_while`

| | |
|---|---|
| Document number: | DxxxxR0 *(draft — no P number has been requested yet)* |
| Date: | 2026-10-16 |
| Project: | ISO JTC1/SC22/WG21: Programming Language C++ |
| Audience: | SG9 Ranges, Library Evolution Working Group (LEWG) |
| Reply-to: | Anish Aggarwal \<anishagg17@gmail.com\> |
| Target: | C++29 |

## Abstract

[P2760R1], *A Plan for C++26 Ranges*, lists `views::trim` as a Tier 2
range adaptor to be ported from range-v3. It has not yet been papered.
This paper proposes `std::views::trim_while`, a range adaptor closure
that removes the maximal leading run and the maximal trailing run of
elements satisfying a predicate, together with `std::views::trim`, the
value-based companion that removes elements equal to a given value.
The two-form split follows the library's `remove`/`remove_if`
convention (§V.2). The name `views::trim_while` is this paper's own
addition: it appears in neither P2760 nor range-v3; range-v3 has the
single name `views::trim`, for the predicate form only.

```cpp
std::string s = "  hello  ";
auto t = s | std::views::trim(' ');                 // "hello"
std::vector<int> v = {0, 0, 7, 8, 0};
auto u = v | std::views::trim_while([](int i) { return i == 0; });
                                                    // u is a view of {7, 8}
```

The adaptor requires a bidirectional, common base range (as in
range-v3); its iterators are the base range's own iterators, with the
two bounds found by a single forward and a single backward scan, so
the result is always a common range and preserves the base's iterator
category. No existing standard adaptor expresses this operation:
`views::drop_while` trims only the leading edge, and the trailing
edge cannot be obtained by composition without giving up the base's
own iterators and its iterator category. Composing
`views::reverse`/`views::drop_while`/`views::reverse` works and is
lazy, but the result is not a `contiguous_range`, and the kept part's
positions in the original range are recoverable only by unwrapping
the composition (§III.1).
The semantics proposed here are range-v3's semantics, which have
shipped since 2019; the divergences from range-v3 (the name of the
predicate form, and sized_range behaviour) are deliberate, argued
below, and put to SG9 as explicit questions.

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

[P2214R2] gave WG21 a prioritised plan for C++23 ranges, and
[P2760R1] did the same for the following cycle. In P2760R1's table
of range-v3 adaptors (§3), `trim` is recorded as "range-v3" status
with "Tier 2" proposed priority, and §8.2 of that paper lists
`views::trim` among the Tier 2 range adaptors. The plan paper offers
no design discussion for this item: it is a work item, waiting for
an author. This paper takes the trim item.

The operation is familiar from string processing: it is what
`str.strip` does in Python, what `str::trim_matches` does in Rust,
and what `boost::algorithm::trim` does for strings. Whitespace
around tokens, zeroes around a signal, sentinel values around a
payload, and empty slots around a sparse array's live region are
all the same shape. In C++ today the operation needs a hand-written
pair of scans, or a `drop_while` for the front plus manual iterator
surgery for the back.

## III. Motivation and scope

### III.1 The gap

Given a bidirectional range and a predicate, obtaining the subrange
that remains after removing the longest matching prefix and the
longest matching suffix currently looks like:

```cpp
auto first = std::ranges::find_if_not(rng, pred);
auto last = std::ranges::end(rng);
while (last != first) {
    auto prev = std::ranges::prev(last);
    if (!pred(*prev)) break;
    last = prev;
}
auto rest = std::ranges::subrange(first, last);
```

This is short but easy to get subtly wrong (the `last != first`
guard: without it, an all-matching range reads before its begin),
and it has to be rewritten at every call site because no adaptor
captures it:

- `views::drop_while(pred)` removes the prefix only.
- C++23's `ranges::find_last_if_not(rng, pred)` finds the trailing
  boundary in one call: it returns the subrange that starts at the
  last element for which `pred` is `false`. The kept range still
  has to be assembled by hand at every call site: advance one past
  that iterator, pair it with the leading bound, and handle the
  all-matching case, where the returned subrange is empty. And the
  result is an algorithm's return value, not a composable adaptor.
- Composing for the suffix (`rng | views::reverse |
  views::drop_while(pred) | views::reverse`) works, and is fully
  lazy; it is a workaround with specific costs, not a substitute.
  The result is not a `contiguous_range` even when `rng` is one
  (verified against libstdc++: the composition preserves
  bidirectional, random-access, sized and common, and loses only
  contiguity). Its iterators are adaptor wrappers rather than the
  base's own iterators, so the kept part's positions in the original
  range are recoverable only by unwrapping the composition:
  `.base().base()` through the two `reverse_view`s yields the
  original iterator, subject to `reverse_iterator`'s off-by-one.
  That is possible, but the composition's type offers nothing
  directly: no offsets, no `subrange` or `string_view` of the kept
  part. And it is a paragraph of explanation at every use.
  (range-v3's own test suite checks `trim` against a composition
  with a single reverse, `ia | views::drop_while(p) |
  views::reverse | views::drop_while(p)`. The comparison can stand
  in for `trim` only because the expected result,
  `{4, 3, 2, 1, 2, 3, 4}`, is a palindrome, so the reversed order
  of the composition's output goes unnoticed.)
- `views::take_while`, `views::drop`, `views::take` are one-ended.

### III.2 Scope

In scope: one view class, `trim_view`, exposed through two range
adaptor objects, `views::trim_while` (predicate) and `views::trim`
(value), in `<ranges>`, following the design of range-v3's
`ranges::views::trim` with the divergences documented in §V.

Out of scope: trimming algorithms that mutate (`ranges::trim` as an
algorithm returning a subrange is a possible future companion, not
proposed here); a "set of values" overload (see §V.2); trimming for
input or forward-only ranges (see §V.3); string-specific member
functions (a separate concern, for SG16/string papers).

## IV. Impact on the standard

The change is a pure library addition to `<ranges>`:

- one new class template, `std::ranges::trim_view`, in a new
  subclause of [range.adaptors];
- two new range adaptor objects, `std::ranges::views::trim_while` and
  `std::ranges::views::trim`;
- a specialisation of `std::ranges::enable_borrowed_range` for
  `trim_view`;
- a feature-test macro, `__cpp_lib_ranges_trim` (proposed; see §V.7).

No existing wording is modified, no core-language change is required,
and nothing is deprecated. The addition is implementable in C++20
terms on top of the existing ranges machinery; a reference
implementation accompanies this paper (§VII).

## V. Design decisions

### V.1 The predicate direction

Throughout, `pred(e) == true` means *e is trimmed*. This matches
`drop_while` (drop while the predicate holds), `remove_if` (remove
when the predicate holds), Boost.StringAlgo's `trim_if`, and
range-v3. The alternative "keep-if" reading (the `filter`
convention) is rejected: an adaptor named `trim_while` whose
predicate selected what to *keep* would contradict the
`drop_while` convention its name invokes. The abstract and the
wording both state the direction explicitly.

### V.2 Names: `trim_while` for the predicate, `trim` for the value

range-v3 names the predicate form `views::trim`. This paper gives
the two forms distinct names, by analogy with the plan's Tier 1
pairs `drop_last`/`drop_last_while` and `take_last`/`take_last_while`
in [P2760R1] (`drop`/`drop_while` and `take`/`take_while` themselves
are C++20 adaptors and carry no priority in the plan):

- `views::trim_while(pred)`: remove from both ends while `pred` holds;
- `views::trim(value)`: remove from both ends while the element
  equals `value` (specified as `trim_while` with an equality
  predicate).

The pairing also follows the library's `remove`/`remove_if`
convention. `std::ranges::remove` and `std::ranges::remove_if` are
algorithms, specified together, neither in terms of the other; the
plan's Tier 1 list carries `views::remove` and `views::remove_if`
as a pair of adaptors, but neither is in the standard yet. The
convention is the library's; this paper does not claim an existing
`views::remove` as precedent.

One question should be answered directly. In the `drop`/`take`
families, the un-suffixed name takes a *count*: `drop(n)`,
`take(n)`. Why does `trim(x)` take a value instead? Because a
count is not something a caller of `trim` can state. Trimming is
defined by what the padding *is*, not by how much of it there is:
the amount of padding is what the scan discovers. Every existing
trim facility identifies the padding, by a predicate, a value, or
a set of characters (Python's `str.strip(chars)`); none takes a
count. A caller who does know the counts already has the counted
adaptors (`views::drop`, and `views::drop_last` as proposed in
[P4294R0]).

The value form is justified by frequency: trimming zeroes,
sentinels, NUL bytes, or spaces (`trim(' ')`) is the common case,
and each reads better than its lambda. The split's cost, divergence
from range-v3 muscle memory, is real, and is put to SG9 as Poll 1.

*History of the name.* This paper was originally drafted, and the
idea floated on std-proposals, with the predicate form named
`trim_if`, following the `remove`/`remove_if` convention. Arthur
O'Dwyer replied on the list that `trim_while` "would be the better
and more appropriate/consistent name". The author agrees: a
trimming predicate is a "while" condition in the `drop_while`
sense, not a per-element selection in the `remove_if` sense. The
paper was renamed before any numbered version: the predicate form
is `trim_while` throughout this draft. What SG9 is asked in Poll 1
is therefore no longer the name itself, but whether the two-form
split (as opposed to range-v3's single overloaded name) is wanted
at all.

A third form, trimming any element of a *set* of values (Python's
`str.strip(chars)`, C#'s `Trim(params char[])`, Rust's
`trim_matches(&[' ', '\t'])`), is **not** proposed in R0. Its generic
form raises questions the predicate form already answers by hand
(what is a "set" for an arbitrary element type, and what lookup
does the adaptor perform?), and it can be added later without
disturbing either proposed form. The paper asks SG9 whether it
wants the set form in scope (Appendix B, Q2).

### V.3 Requirements: bidirectional and common; no degraded mode

`trim_view` requires the base range to model `bidirectional_range`
and `common_range`, and `pred` to model
`indirect_unary_predicate<iterator_t<V>>`: exactly range-v3's
requirements.

- *Bidirectional* is intrinsic: the trailing bound is found by
  walking backwards from the end. For a forward-only range there is
  no lazy trailing trim at all (any implementation must buffer or
  re-scan), and silently trimming only the front would be a
  correctness trap: the caller's trailing padding survives, with no
  diagnostic. Users with forward ranges already have the one-ended
  tool, `views::drop_while(pred)`. Applying `trim_while` to a
  forward-only range is therefore a compile-time constraint failure,
  demonstrated in the reference implementation's tests.
- *Common* is needed because the backward scan must start from an
  iterator to the end; a sentinel cannot be decremented. The
  existing remedy for non-common ranges, piping through
  `views::common` first, applies unchanged. Relaxing this (by
  common-ising internally) is listed as an open question
  (Appendix B, Q3).

### V.4 Mechanism and concept profile

The view stores the base view and the predicate. The predicate sits
in a *movable-box*, the exposition-only wrapper `drop_while_view`
uses, so that a predicate which is constructible but not assignable
(a capturing lambda is the everyday case) still yields a view that
models `view` and can be piped into further adaptors. `begin()` is
the first position whose element does not satisfy `pred`;
`end()` is one past the last such position, found by scanning
backwards from the end and never passing `begin()`. Both bounds are
computed at most once and cached (the standard scanner-adaptor
pattern, cf. `drop_while_view`'s cached begin).

Because the bounds *are* base iterators, no iterator wrapper class
is needed:

| Property | Behaviour |
|---|---|
| `view` | yes |
| `common_range` | always (begin and end have the base iterator type) |
| `bidirectional_range` | iff the base is bidirectional (always, by the constraint) |
| `random_access_range` | iff the base is random-access |
| `contiguous_range` | iff the base is contiguous |
| `borrowed_range` | iff the base is borrowed (`enable_borrowed_range` propagates) |
| `sized_range` | iff the base's iterator/sentinel pair is a `sized_sentinel` pair — see §V.5 |

Construction of the view is O(1). The first `begin()` costs a scan
of the trimmed prefix, the first `end()` a scan of the trimmed
suffix; afterwards both are O(1). Elements
kept are visited exactly as in the base range; nothing is copied.

There are deliberately no const `begin()`/`end()`. A const overload
could only recompute the bounds on every call, and `begin()` for a
range is required to be amortized constant time: a per-call rescan
of the trimmed runs would break that requirement. `drop_while_view`
makes the same choice: it has no const `begin()` at all. A `const
trim_view` is therefore not itself a range.

### V.5 sized_range: follow `drop_while_view`, not range-v3

range-v3's `trim_view` is never a `sized_range`
(`disable_sized_range` is specialised to `true`). This paper
proposes the opposite default, on a standard-internal precedent:
`drop_while_view`, the nearest structural relative, whose iterators
are likewise the base's, **does** model `sized_range` over, e.g., a
`vector` base in existing implementations, because
`view_interface::size()` (end − begin) is available and yields the
correct post-adaptation size. `take_while_view` fails to model it
only because its sentinel is a distinct type, which is not our
situation: `trim_view`'s end is an iterator.

Accordingly: `trim_view` models `sized_range` exactly when the
base's iterator/sentinel pair models `sized_sentinel_for`, and
`ranges::size` then returns the exact trimmed size. (An early draft
of the reference implementation specialised `disable_sized_range`
to imitate range-v3. That does not change the result:
`ranges::size` falls back to `end() - begin()` for a common range
whose iterator/sentinel pair is a sized-sentinel pair, and
`disable_sized_range` does not suppress that fallback; the same
behaviour is seen with libc++. The specialisation was removed.)

Computing `size()` requires both bounds, so the first `size()` call
on a view pays for the two scans; with the bounds cached, later
calls are O(1), the same profile as `drop_while_view`'s `size()`.
If SG9 prefers range-v3's stricter profile, the mechanism is a
distinct sentinel type for the view, a materially bigger design,
recorded as an alternative in Appendix A.

### V.6 No projection in R0

range-v3's `trim` accepts an optional projection, composed with the
predicate. The standard's one-sided relatives (`drop_while`,
`take_while`, `filter`) do not. For consistency with that family,
R0 proposes no projection; `views::transform` before the trim covers
the use case. Recorded as an open question (Appendix B, Q4) in case
SG9 sees it differently.

### V.7 constexpr and the feature-test macro

All of `trim_view`'s constructors and member functions, and both
adaptor objects' call operators, are `constexpr`, so the adaptors
are usable in constant expressions wherever the base range is
(demonstrated in the reference implementation over `std::array`).

Feature-test macro: this paper proposes a new macro
**`__cpp_lib_ranges_trim`**, value `2026XXL`, defined by `<ranges>`
and `<version>`. Whether a per-feature macro or a bump of
`__cpp_lib_ranges` is preferred is an SD-6 matter; the choice and
the value's digits are placeholders for SG10/LWG to settle, and are
flagged as such in the wording.

## VI. Technical specification (draft wording)

*Editorial note: the following is a **wording sketch** in the style
of [range.adaptors], prepared for design review. It has not been
reviewed by LWG, paragraph numbering is relative, and bracketed
notes mark the places where LWG input is expected. It is written
against the C++26 working draft's structure for [range.adaptors].*

### VI.1 Header `<ranges>` synopsis

Add to namespace `std::ranges`, in the range adaptors synopsis:

```cpp
// [range.trim], trim view
template<view V, class Pred>
  requires bidirectional_range<V> && common_range<V> && is_object_v<Pred> &&
           indirect_unary_predicate<const Pred, iterator_t<V>>
class trim_view;

namespace views {
  inline constexpr /* unspecified */ trim_while = /* unspecified */;
  inline constexpr /* unspecified */ trim = /* unspecified */;
}
```

and the borrowed-range specialisation:

```cpp
template<class V, class Pred>
  inline constexpr bool enable_borrowed_range<trim_view<V, Pred>>
    = enable_borrowed_range<V>;
```

### VI.2 New subclause [range.trim]

> **26.7.x Trim view [range.trim]**
>
> **26.7.x.1 Overview [range.trim.overview]**
>
> (1) `trim_view` takes a bidirectional common range and a predicate,
> and produces a view of the elements of the range with the maximal
> leading run and the maximal trailing run of elements satisfying
> the predicate removed.
>
> (2) The name `views::trim_while` denotes a range adaptor object
> ([range.adaptor.object]). Given subexpressions `E` and `P`, the
> expression `views::trim_while(E, P)` is expression-equivalent to
> `trim_view(E, P)`.
>
> (3) The name `views::trim` denotes a range adaptor object. Given
> subexpressions `E` and `X`, the expression `views::trim(E, X)` is
> expression-equivalent to `views::trim_while(E, P)`, where `P` is
> an object of an exposition-only class whose instances store a
> decayed copy of the value `X` (as if by `DECAY-COPY(X)`) and
> whose function call operator tests an element for equality with
> the stored value. (Storing the value, rather than referring to
> `X`, is essential: in
> `auto t = r | views::trim(' ');` the adaptor object outlives the
> full expression, so a predicate referring to the argument would
> dangle.) *[Note: the final form of the equality predicate —
> including the constraint that the element type be comparable with
> the value type, checked by the adaptor's own constraints so that a
> non-comparable element type is a substitution failure rather than
> an error inside the view — is for LWG to settle; the intent is
> "elements equal to the value". — end note]*
>
> *Example:*
> ```cpp
> std::string s = "  hello  ";
> auto t = s | views::trim(' ');   // t is a view of "hello"
> std::vector<int> v = {0, 0, 1, 2, 0};
> auto u = v | views::trim(0);     // u is a view of {1, 2}
> ```
>
> **26.7.x.2 Class template `trim_view` [range.trim.view]**
>
> ```cpp
> namespace std::ranges {
>   template<view V, class Pred>
>     requires bidirectional_range<V> && common_range<V> && is_object_v<Pred> &&
>              indirect_unary_predicate<const Pred, iterator_t<V>>
>   class trim_view : public view_interface<trim_view<V, Pred>> {
>   private:
>     V base_ = V();                                    // exposition only
>     movable-box<Pred> pred_;                          // exposition only
>     non-propagating-cache<iterator_t<V>> begin_;      // exposition only
>     non-propagating-cache<iterator_t<V>> end_;        // exposition only
>   public:
>     trim_view()
>       requires default_initializable<V> && default_initializable<Pred>
>       = default;
>     constexpr explicit trim_view(V base, Pred pred);
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
>     trim_view(R&&, Pred) -> trim_view<views::all_t<R>, Pred>;
> }
> ```
>
> `constexpr explicit trim_view(V base, Pred pred);`
> (4) *Effects:* Initializes `base_` with `std::move(base)` and
> `pred_` with `std::move(pred)`.
>
> `constexpr iterator_t<V> begin();`
> (5) *Preconditions:* `pred_.has_value()` is `true`.
> (6) *Effects:* If `begin_` does not contain a value, initializes
> it with `ranges::find_if_not(base_, cref(*pred_))`.
> (7) *Returns:* `*begin_`.
>
> `constexpr iterator_t<V> end();`
> (8) *Preconditions:* `pred_.has_value()` is `true`.
> (9) *Effects:* If `end_` does not contain a value, initializes
> it as follows: let `last` be `ranges::end(base_)`; while `last` is
> not equal to `begin()`, let `p` be `ranges::prev(last)`; if
> `bool(std::invoke(*pred_, *p))` is `false`, stop; otherwise set
> `last` to `p`. Initializes `end_` with `last`.
> (10) *Returns:* `*end_`.
>
> *[Note: the caches are* non-propagating-cache*s, as with
> `drop_while_view`: copying or moving a `trim_view` does not carry
> the cached iterators into the new view, and a move also empties
> the source's caches. A cached iterator may
> point into the source view object itself (e.g. when the base is
> `views::single`, which stores its element inside the view), so
> propagating it would leave the copy with an iterator into the
> original, a dangling iterator once the original is destroyed.
> Whether the caches should instead be specified behaviourally
> ("begin() may cache its result") is for LWG; the observable
> semantics are identical either way. — end note]*

### VI.3 Feature-test macro

Add to the feature-test macro table ([version.syn], and `<ranges>`):

```cpp
#define __cpp_lib_ranges_trim 2026XXL // also in <ranges>
```

*[Value and the new-macro-vs-`__cpp_lib_ranges`-bump choice are
placeholders; see §V.7.]*

## VII. Implementation experience

A portable C++20 reference implementation accompanies this paper
(`std_proposal::ranges::trim_view`, header-only, in namespace
`std_proposal` rather than `std`). It implements the design of §V
exactly, including the constraint set of §V.3 (misuse on a
forward-only or non-common range is a substitution failure, checked
by concept-based tests), the cached-bounds mechanism of §V.4, and
borrowed-range propagation.

As of 13 Oct 2026: the test suite compiles
warning-free under `-Wall -Wextra -Werror` and runs to `PASS` in all
four configurations: g++ 13.3.0 and clang++ 18.1.3 (libstdc++),
each at `-std=c++20` and `-std=c++23`. The suite covers the boundary
behaviours (empty range, all elements trimmed, single element),
the range-v3 test case of [range-v3], composition with
`transform`/`filter`/`reverse`, the concept profile of §V.4
(including borrowed-range propagation and the sized_range profile
of §V.5), concept-based negative tests showing forward-only and
non-common bases are rejected, and `static_assert` evaluation over
`std::array`. Exact
commands and outputs: `impl/verification-transcript-2026-10-13.txt`
(the original runs) and `impl/verification-transcript-2026-10-16.txt`
(the suite re-run on 2026-10-16 after the rename of §V.2, with the
results unchanged).

Two design points were settled by building the implementation: the
sized_range behaviour of §V.5, and the constrained default
constructor in §VI.2 (an unconstrained one is a hard error, not a
clean concept failure, inside Clang's C++23 adaptor machinery for
non-default-initialisable bases such as `ref_view`). The
movable-box, and the non-propagating caches whose move operations
empty the source cache, are likewise required by the design: the
box is what lets a capturing-lambda predicate yield a view that
models `view` and can be piped onward, and the cache behaviour is
what keeps a populated moved-from view from iterating freed
storage; the cache regression tests run under AddressSanitizer.
In C++23 the adaptor
closure types derive from `std::ranges::range_adaptor_closure`, so
closures compose before application (`views::trim(' ') |
views::reverse`); C++20 provides range | closure only. No performance
claims are made: the adaptor's costs are the base range's plus two
bounded scans, and no benchmark programme has been run.

## VIII. Polls requested

1. **Naming:** forward the two-form split of §V.2,
   `views::trim_while(pred)` (predicate) and `views::trim(value)`
   (value), to LEWG, diverging from range-v3's single
   `views::trim(pred)`? *(Alternative: keep range-v3's single name
   for both forms, or drop the value form.)* The predicate form was
   floated as `trim_if` and renamed to `trim_while` following
   std-proposals feedback (§V.2); the split itself is what this
   poll asks about.
2. **Requirements:** require `bidirectional_range` and
   `common_range`, with no forward-only degraded mode, as in §V.3?
3. **sized_range:** adopt the `drop_while_view` precedent of §V.5
   (model `sized_range` when the base iterator/sentinel pair is a
   sized-sentinel pair) rather than range-v3's never-sized?
4. **Direction:** forward this design to LEWG for C++29?

## IX. Acknowledgements

Thanks to Barry Revzin for the plan entry in [P2760R1] that this
paper implements, and to Arthur O'Dwyer for suggesting the
`trim_while` name on std-proposals. Thanks also to Johel Guerrero
and the range-v3 contributors for seven years of shipped semantics
to build on. The design divergences from range-v3 in §V are this
paper's, not theirs.

AI tools were used to help draft this paper and its reference
implementation; the author has reviewed both.

## X. References

- [P2760R1] *A Plan for C++26 Ranges.* <https://wg21.link/p2760r1>
- [P2214R2] *A Plan for C++23 Ranges.* <https://wg21.link/p2214r2>
- [P4294R0] *views::take_last and views::drop_last.*
  <https://wg21.link/p4294r0>
- [range-v3] Eric Niebler et al., *range-v3*, `views::trim`:
  <https://github.com/ericniebler/range-v3/blob/master/include/range/v3/view/trim.hpp>
  and its tests, `test/view/trim.cpp` in the same repository.
- Boost.StringAlgo, `boost::algorithm::trim` / `trim_if`:
  <https://www.boost.org/doc/libs/release/doc/html/string_algo.html>
- Rust standard library, `str::trim`, `str::trim_matches`:
  <https://doc.rust-lang.org/std/primitive.str.html>

## Appendix A: Alternatives considered

- **A `ranges::trim` algorithm** returning a `subrange`: plausible
  and arguably complementary, but the plan item is the adaptor, and
  adaptors compose; an algorithm can be proposed separately on the
  same semantics.
- **Distinct sentinel type / never-sized:** the range-v3 profile.
  Rejected for §V.5's reasons (the `drop_while_view` precedent and
  the correctness of the inherited `size()`), but available to SG9
  as Poll 3's alternative.
- **Forward-only support by buffering:** a different adaptor (owning,
  with weaker iterator guarantees). Rejected: it is not the plan
  item, and its semantics deserve their own paper if wanted.
- **Predicate direction inverted ("keep-if"):** rejected in §V.1.
- **Single name `views::trim` for both forms (range-v3
  compatible), value form omitted or named `trim_value`:** the main
  alternative to §V.2; covered by Poll 1. The `trim_if` name for the
  predicate form (the `remove`/`remove_if` convention) was the
  original draft's choice and the name used in the std-proposals
  float; it was dropped in favour of `trim_while` for §V.2's
  reasons.

## Appendix B: Open questions

- Q1. Naming (Poll 1): the two-form split of §V.2 vs
  range-v3's single name. (The predicate form's name itself
  was settled as `trim_while` after the float; it was
  floated as `trim_if`.)
- Q2. Should a set-of-values form (Python `strip(chars)` style) be
  in scope for R0, and if so what are its lookup semantics?
- Q3. Should the `common_range` requirement be relaxed by
  common-ising internally, at the cost of a wrapper iterator?
- Q4. Should a projection be supported (range-v3 has one; the
  standard's one-sided family does not)?
- Q5. Feature-test macro: new `__cpp_lib_ranges_trim` vs a bump of
  `__cpp_lib_ranges` (SG10/SD-6 steer).

## Revision history

- **DxxxxR0 (2026-10-13):** initial draft for SG9 discussion.
  Circulated as a D-number draft; no P number requested yet.
- **DxxxxR0, rename (2026-10-16, unpublished edit):** the predicate
  form was renamed `trim_if` → `trim_while` throughout, following
  feedback on the std-proposals float (§V.2). No semantic change.
