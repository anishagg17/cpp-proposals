# views::trim / views::trim_while

Draft WG21 proposal (DxxxxR0) to add `std::ranges::views::trim` and
`std::ranges::views::trim_while` — a pair of range adaptors that remove
the maximal leading and trailing runs of elements from a range:
`trim` removes elements equal to a value, `trim_while` removes
elements satisfying a predicate. `views::trim` is a Tier 2 item in
P2760 (*A Plan for C++26 Ranges*), to be ported from range-v3; the
two-name split is this proposal's own (see paper §V.2).

The predicate form was floated on std-proposals as `trim_if` and
renamed to `trim_while` after feedback on the list.

Author: Anish Aggarwal <anishagg17@gmail.com>

## Contents

- `paper/draft-paper.md` — the draft proposal: design decisions with
  rationale, a wording sketch for a new [range.trim] subclause, and
  polls for SG9.
- `impl/` — a reference implementation (`views_trim.hpp`) with tests.
  Verified with g++ 13.3 and clang++ 18.1 at C++20 and C++23,
  ASan/UBSan clean. Build and run with `make` in `impl/`.
- `research/` — the design research and duplicate check behind the
  paper (P2760 entry, range-v3 semantics, prior art).

## Status

Floated on std-proposals on 2026-10-13 (as `trim` / `trim_if`). The
predicate form was renamed to `trim_while` following the list's
feedback; trim is otherwise unchanged. A P-number request follows in
due course.

## Licence

MIT — see the repository root `LICENSE`.
