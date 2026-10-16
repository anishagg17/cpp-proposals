#!/bin/sh
# Build and run the views::drop_last_while / views::take_last_while
# tests under both compilers and both standard modes. Any failure
# aborts with a non-zero exit status. On this VM the Clang binary is
# clang++-18 (18.1.3); plain clang++ is not on PATH.
set -e
cd "$(dirname "$0")"

for cc in g++ clang++-18; do
    for std in c++20 c++23; do
        bin="test_${cc}_${std}"
        echo "== $cc -std=$std =="
        "$cc" -std=$std -Wall -Wextra -Werror test_views_drop_last_while.cpp -o "$bin"
        "./$bin"
    done
done
echo "ALL CONFIGURATIONS PASS"
