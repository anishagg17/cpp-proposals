# C++ library verifications — 2026-10-15 (index/doc reads, second pass)

## LLVM StringRef — VERIFIED (docs mirror, LLVM 15.x)
URL: https://docs.hdoc.io/hdoc/llvm-project/rECFCAF624C04523A.html
Method overview of llvm::StringRef shows:
- `rtrim(char)`, `rtrim(StringRef Chars = " \t\n\v\f\r")`, `ltrim(...)`, `trim(...)` — char / char-set based, string-specific.
- `drop_while(function_ref<bool(char)>)`, `take_while(function_ref<bool(char)>)` — predicate-based but FRONT only; no drop_back_while / take_back_while.
- `drop_back(size_t N = 1)`, `take_back(size_t N = 1)` — count-based only.
- `find_last_not_of(char)`, `find_last_not_of(StringRef Chars)` — direct analogue of std:: find_last_not_of.
Conclusion: LLVM has every neighbouring facility except a predicate-based end-while; users compose rtrim (char-set only) or manual reverse find_if.

## Abseil — VERIFIED (source mirrors of absl/strings/ascii.h)
URL: https://chromium.googlesource.com/chromium/src/third_party/abseil-cpp/+/refs/heads/main/absl/strings/ascii.h
- `absl::StripTrailingAsciiWhitespace(string_view)` implemented literally as the manual idiom:
  `std::find_if_not(str.rbegin(), str.rend(), absl::ascii_isspace)` then substr/erase. Whitespace-only, string-specific; also `StripSuffix` (exact suffix, not a predicate run). No generic range adaptor.

## Bonus use case — Google protobuf (Fuchsia mirror, pinned commit)
URL: https://fuchsia.googlesource.com/third_party/protobuf/+/0196d1e1387a903ee0cc44b301bb94f7ce7d5d2a/src/google/protobuf/compiler/objectivec/helpers.cc
- Objective-C code generator drops trailing empty comment lines with a hand loop:
  `while (!raw_lines.empty() && raw_lines.back().empty()) raw_lines.pop_back();`
  and strips trailing whitespace per line via absl::StripTrailingAsciiWhitespace. Manual drop_last_while in a major codebase.

## Boost.StringAlgorithms — VERIFIED (Boost 1.82 docs)
URL: https://original.boost.org/doc/libs/1_82_0/doc/html/boost/algorithm/trim_right_copy_if.html
- `boost::algorithm::trim_right_copy_if` / `trim_right_if`: "Right trim - parametric" — arbitrary unary predicate, but eager algorithms on sequences/strings (copy or in-place erase via detail::trim_end), not lazy range views; string-algorithm library scope.

## Folly — NOT verified this pass
Searches returned no authoritative Folly page for a trim facility. Report states: not verified; no generic range adaptor claimed either way.

## Scala — bounded conclusion
- javadoc.io listing of Scala 2.13.0-RC3 scala.collection.immutable.Seq (flattened incl. inherited members) shows in alphabetical order: drop(n), dropRight(n), dropWhile(p) — no dropRightWhile between dropRight and dropWhile. (Listing URL not captured verbatim; do not cite a URL for this specific check.)
- Scala 3.3 LTS View API has count-based DropRight only: https://www.scala-lang.org/files/archive/api/3.3_LTS/scala/collection/View$$DropRight.html
- Long-standing request: https://github.com/scala/bug/issues/4247
Conclusion for report: absent from stdlib (2.13 checked as above; 3.x not exhaustively verified); users compose via reverse/lastIndexWhere.
