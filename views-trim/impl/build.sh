#!/bin/sh
# Build and run the views::trim / views::trim_while tests under both
# compilers and both standard modes. Any failure aborts with a
# non-zero exit status.
set -e
cd "$(dirname "$0")"

for cc in g++ clang++; do
    for std in c++20 c++23; do
        bin="test_${cc}_${std}"
        echo "== $cc -std=$std =="
        "$cc" -std=$std -Wall -Wextra -Werror test_views_trim.cpp -o "$bin"
        "./$bin"
    done
done
echo "ALL CONFIGURATIONS PASS"
