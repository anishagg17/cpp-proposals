#!/usr/bin/env bash
# Builds and runs both test files with every available compiler/standard
# combination, plus sanitizer builds. Run from views-trim/:  verification/run-all.sh
set -u
cd "$(dirname "$0")/../impl"
out="../verification/results-$(date -u +%Y-%m-%d).txt"
pass=0; fail=0; xfail=0
# Known toolchain issue (not this library): Clang 20 with libstdc++ 14 in
# C++23 mode cannot call a composed pipe whose left side is a class-template
# closure derived from std::ranges::range_adaptor_closure. Reduced
# reproducer, with no trim code at all: verification/toolchain-issue-clang20-libstdcxx14.cpp
# (passes on GCC 14, on Clang 20 with libc++, and on Clang 18 with libstdc++ 13).
is_known_issue() { case "$1" in *test_views_trim.cpp*clang++-20*-std=c++2[3c]*) case "$1" in *libc++*) return 1;; esac; return 0;; esac; return 1; }
run() { # label, command...
  local label="$1"; shift
  printf '\n$ %s\n' "$*" >>"$out"
  if "$@" >>"$out" 2>&1 && ./a.out >>"$out" 2>&1; then
    echo "PASS  $label" | tee -a "$out"; pass=$((pass+1))
  elif is_known_issue "$label"; then
    echo "XFAIL $label  (known Clang 20 + libstdc++ 14 issue, see run-all.sh)" | tee -a "$out"; xfail=$((xfail+1))
  else
    echo "FAIL  $label" | tee -a "$out"; fail=$((fail+1))
  fi
  rm -f a.out
}
{ echo "P4398R0 reference implementation: verification run, $(date -u '+%Y-%m-%d %H:%M UTC')"; echo; } >"$out"
for c in g++ g++-14 clang++ clang++-20; do command -v $c >/dev/null && $c --version | head -1 >>"$out"; done
W="-Wall -Wextra -Werror -I."
for t in test_views_trim.cpp test_edge_cases.cpp; do
  for c in g++ g++-14 clang++ clang++-20; do
    command -v $c >/dev/null || continue
    for s in c++20 c++23 c++2c; do
      [ "$c" = g++ ] && [ "$s" = c++2c ] && continue   # GCC 13 has no -std=c++2c
      run "$t  $c -std=$s" $c -std=$s $W $t
    done
  done
  command -v clang++-20 >/dev/null && run "$t  clang++-20 -std=c++2c -stdlib=libc++" clang++-20 -std=c++2c -stdlib=libc++ $W $t
  command -v g++-14 >/dev/null && run "$t  g++-14 -std=c++2c ASan+UBSan" g++-14 -std=c++2c -g -fsanitize=address,undefined -fno-sanitize-recover=all $W $t
  command -v clang++-20 >/dev/null && run "$t  clang++-20 -std=c++2c ASan+UBSan" clang++-20 -std=c++2c -g -fsanitize=address,undefined -fno-sanitize-recover=all $W $t
done
# The code examples in the paper itself
for c in "g++-14 -std=c++2c" "clang++-20 -std=c++2c -stdlib=libc++" "clang++ -std=c++23"; do
  command -v ${c%% *} >/dev/null && run "paper-examples.cpp  $c" $c $W ../verification/paper-examples.cpp
  command -v ${c%% *} >/dev/null && run "trim-equal-to.cpp  $c" $c $W ../verification/trim-equal-to.cpp
done
printf '\nTotal: %d passed, %d failed, %d known toolchain issue\n' $pass $fail $xfail | tee -a "$out"
echo "Reduced reproducer for the toolchain issue:" >>"$out"
for c in "clang++-20 -std=c++23" "clang++-20 -std=c++23 -stdlib=libc++" "g++-14 -std=c++23"; do
  printf '  %-40s ' "$c" >>"$out"; $c ../verification/toolchain-issue-clang20-libstdcxx14.cpp -o /dev/null >/dev/null 2>&1 && echo compiles >>"$out" || echo "fails" >>"$out"
done
[ $fail -eq 0 ]
