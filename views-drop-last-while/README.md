# views::drop_last_while / views::take_last_while

Draft WG21 proposal (DxxxxR0) to add the predicate-based trailing-run
range adaptors `std::ranges::views::drop_last_while` and
`std::ranges::views::take_last_while`, the trailing-run
counterparts of `views::drop_while` / `views::take_while`. Listed as Tier 1 in P2760
(*A Plan for C++26 Ranges*); the counted counterparts
(`views::drop_last` / `views::take_last`) are proposed in P4294.

Author: Anish Aggarwal <anishagg17@gmail.com>

## Contents

- `paper/draft-paper.md` — the draft proposal: motivation with
  cross-language and public-code frequency evidence, design, wording
  sketch, and open polls.
- `impl/` — a reference implementation (`views_drop_last_while.hpp`)
  with tests. Verified with g++ 13.3 and clang++ 18.1 at C++20 and
  C++23, ASan/UBSan clean. Build and run with `make` in `impl/`.
- `research/` — the use-case and frequency research behind the
  motivation section (language survey, concrete use cases in real
  codebases, and a two-index public-code frequency study).

## Status

Floated on std-proposals on 2026-10-14. Hewill Kang replied on
2026-10-15: "As far as I know, no one in SG9 is working on this."
A P-number request follows in due course.

## Licence

MIT — see the `LICENSE` file at the root of this repository.
