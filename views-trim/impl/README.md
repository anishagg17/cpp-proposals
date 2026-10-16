# Reference implementation — `std::views::trim` / `std::views::trim_while`

Note on the name: the predicate form was called `trim_if` in the
first draft and in the std-proposals float. It was
renamed to `trim_while` after feedback on the list (Arthur
O'Dwyer suggested `trim_while` as the more consistent name). Only
the name changed; the semantics are the same.

Proposal context: implement the P2760R1 Tier 2 plan item `views::trim`
(SG9 ranges plan) as a standard range adaptor. The adaptor exists
today only in range-v3 (`ranges::views::trim`). This directory is the
working reference implementation and its evidence; it is **not** the
paper.

## Files

| File | What it is |
|---|---|
| `views_trim.hpp` | Header. `std_proposal::ranges::trim_view` plus the adaptor objects `std_proposal::views::trim_while` / `std_proposal::views::trim` (deliberately **not** in `namespace std`; the only std touch is an `enable_borrowed_range` specialisation for this header's own type, which the standard permits). |
| `test_views_trim.cpp` | Concept/`static_assert` checks + runtime `assert` suite (see coverage list below). Prints `PASS`. |
| `Makefile`, `build.sh` | Build/run recipes for all four compiler/mode combinations. |
| `verification-transcript-2026-10-13.txt` | Exact commands and outputs of the original verification runs, plus two supporting probes cited by the paper (the `drop_while_view` sized_range and const-`begin()` probes). |
| `verification-transcript-2026-10-16.txt` | Exact commands and outputs of the verification runs for the renamed (`trim_while`) build. |

## Semantics implemented (the paper's recommended design)

- `views::trim_while(pred)`: removes the maximal leading run and
  the maximal trailing run of elements for which `pred(element)`
  is true. (`pred` means "trim this", as in `drop_while` and
  range-v3.)
- `views::trim(value)`: removes elements equal to `value` from both
  ends; implemented on top of `trim_while`.
- Base must model `bidirectional_range` + `common_range` (range-v3's
  requirements). Forward-only or non-common bases are compile-time
  constraint failures, checked by concept-based negative tests, not
  just asserted in prose.
- The view's iterators are the base's iterators; both bounds are
  computed by a single scan and cached on the non-const path. The
  view is always a `common_range`, preserves bidirectional /
  random-access / contiguous iteration of the base, and propagates
  `borrowed_range`.
- `sized_range` is modelled exactly when the base's
  iterator/sentinel pair is a sized-sentinel pair, with
  `ranges::size` returning the exact trimmed size: the
  `drop_while_view` precedent, **not** range-v3 (which disables
  sized_range for its trim_view). See "Design history" below.
- No projection parameter (range-v3 has one; the standard's
  one-sided predicate adaptors do not). Everything is `constexpr`
  where the base is.

## Test coverage

- Behaviour: empty range; all elements trimmed; none trimmed;
  leading-only; trailing-only; single element kept; single element
  trimmed; the range-v3 test case (`{0,1,2,3,4,3,2,1,2,3,4,3,2,1,0}`
  with `i < 4` → `{4,3,2,1,2,3,4}`); string trimming by value and by
  predicate; `std::list` (bidirectional, non-random-access) base;
  backward iteration via `views::reverse`.
- Composition: `transform | trim_while`, `trim_while | transform`,
  `filter | trim_while`; two-argument and pipe forms agree.
- Concept profile (static_asserts): `view`, `common_range`,
  `bidirectional_range`, `random_access_range`, `contiguous_range`
  over an array base; `sized_range` present for array/vector bases,
  absent for a `list` base; `borrowed_range` for span and
  lvalue-vector (ref_view) bases, absent for an owning
  (owning_view) base.
- Negative tests (concepts): `trim_while`/`trim` are **not applicable**
  to a `std::forward_list` (forward-only) nor to a bidirectional but
  non-common range (counted iterator + default sentinel).
- `constexpr`: full evaluation in `static_assert`, using a mutable
  local view object over a namespace-scope `constexpr std::array`.
  (trim_view, like `drop_while_view`, has no const `begin()`/`end()`,
  so a const view object is not iterable; constant *evaluation* is
  unaffected. The array sits at namespace scope because a `ref_view`
  over a *local* constexpr array is rejected by GCC 13 even for the
  standard's own `views::take`; that toolchain note is recorded in
  the test file.)
- `view_interface` surface: `front`, `back`, `operator[]`, `empty`,
  `ranges::size`, `pred()` accessor.

## Compilers used

- `g++` — GCC 13.3.0 (Ubuntu 13.3.0-6ubuntu2~24.04.1)
- `clang++` — Clang 18.1.3 (1ubuntu1), using libstdc++ 13

## Exact commands

```sh
g++     -std=c++20 -Wall -Wextra -Werror test_views_trim.cpp -o test_g++_c++20         && ./test_g++_c++20
g++     -std=c++23 -Wall -Wextra -Werror test_views_trim.cpp -o test_g++_c++23         && ./test_g++_c++23
clang++ -std=c++20 -Wall -Wextra -Werror test_views_trim.cpp -o test_clang++_c++20     && ./test_clang++_c++20
clang++ -std=c++23 -Wall -Wextra -Werror test_views_trim.cpp -o test_clang++_c++23     && ./test_clang++_c++23
# or: make test   /   ./build.sh
```

## Test output (real, from the runs above)

All four configurations compile warning-free under `-Werror` and the
test binary prints `PASS` in each: g++ C++20, g++ C++23, clang++
C++20, clang++ C++23. Full verbatim output is in the
verification transcripts listed above.

## Design history — two things the tests changed

1. **sized_range.** The first draft of the header specialised
   `std::ranges::disable_sized_range<trim_view<…>> = true` to imitate
   range-v3. The concept tests showed `sized_range` was satisfied
   anyway: `ranges::size` falls back to `end() - begin()` for a
   common range whose iterator/sentinel pair is a sized-sentinel
   pair, and the `disable_sized_range` specialisation does not
   suppress that fallback (the same behaviour occurs with libc++).
   A probe of the standard's own adaptors (in the 2026-10-13
   transcript) showed `drop_while_view` over a `vector` models
   `sized_range` for exactly this reason, while `take_while_view`
   does not (distinct sentinel type). The design was changed to
   follow `drop_while`: sized exactly when the base's
   iterator/sentinel pair is a sized-sentinel pair, with the correct
   trimmed size. The no-op specialisation was removed rather than
   kept as decoration.
2. **Constrained default constructor.** The first draft had
   `trim_view() = default;`. Under clang++ `-std=c++23`, piping a
   trim view into `views::reverse` then failed with a hard error:
   libstdc++'s C++23 adaptor machinery evaluates
   `default_initializable<trim_view<ref_view<…>, …>>` in an
   exception-specification context, which instantiated the base
   member's `= V()` initialiser for the non-default-initialisable
   `ref_view`. libstdc++'s own `drop_while_view` shows the fix:
   its default constructor carries
   `requires default_initializable<V> && default_initializable<Pred>`
   (which is also what the standard's wording specifies), and
   adopting it made all four configurations pass.
