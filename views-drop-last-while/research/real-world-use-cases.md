# Real-world use cases for `views::drop_last_while` / `views::take_last_while`

Research date: 15 October 2026. Prepared in response to Hewill Kang's reply on the
std-proposals float for `views::drop_last_while` / `views::take_last_while`
(P2760 Tier 1), which asked a paper to explain why these views are worth
standardising, with evidence of how often the pattern occurs in real projects
and what other languages provide.

## Definitions and evidence labels

- `drop_last_while(pred)` — the range with the maximal trailing run of elements
  satisfying `pred` removed.
- `take_last_while(pred)` — only that maximal trailing run.
- The two are complements: for a given range and predicate, one is the prefix
  and the other is the suffix at the same boundary.

Every example below carries one of these labels:

- **Direct** — the operation itself was seen in primary source code, official
  documentation, or a normative specification during this research.
- **Direct (behaviour) / inferred (implementation)** — the behaviour or API
  contract is documented at a primary source, but the internal loop was not
  inspected.
- **Spec-only / inferred** — a format or specification establishes that a
  trailing run exists (for example padding), but no looping strip
  implementation was inspected. These are **not** presented as code evidence.
- **Demand evidence** — a documented gap or issue requesting the operation,
  not an implementation.

A precision rule is applied throughout: removing one fixed suffix
(`TrimSuffix`, `removesuffix`, `strip_suffix`, stripping a single trailing
newline) is **not** counted as a while-run operation unless the source loops
over a run.

## A necessary design observation

The operation is end-anchored. On a bidirectional range the boundary can be
found by scanning backwards over the trailing run only. On a forward-only
range the whole range must be traversed (and, for a view, effectively
buffered or re-scanned) before the first kept element is known. Every mature
precedent below is consistent with this: the operations live on arrays,
strings, bidirectional collections, or materialised buffers — never on
single-pass streams. A WG21 paper should present these as reverse-scan
adaptors for bidirectional ranges, not as streaming filters.

---

## 1. Exact generic precedents — both operations, general element types

### 1.1 Kotlin standard library — `dropLastWhile` / `takeLastWhile` — Direct

- Source: [Kotlin docs — Retrieve collection parts](https://kotlinlang.org/docs/collection-parts.html)
- Operation: The official guide defines the predicate forms alongside
  `take`/`takeLast`/`drop`/`dropLast`: `takeLastWhile` takes the range of
  elements matching the predicate from the end, and `dropLastWhile` is its
  complement, returning everything from the beginning up to the last
  non-matching element. Overloads exist for collections, arrays, and
  character sequences, and have been in the standard library since Kotlin 1.0.
- Mapping: exact, both directions, with the same "last" naming the C++
  proposal uses.
- Implemented today: eager standard-library functions that build a new
  list/string.
- Adaptor verdict: **strong precedent, and the C++ form improves on it.** The
  semantics and naming are proven in a mainstream standard library; a lazy
  C++ view provides the same semantics without the allocation and composes
  in pipelines.

### 1.2 Apple Swift Algorithms — `trimmingSuffix(while:)` / `suffix(while:)` — Direct

- Sources: [Trim guide](https://github.com/apple/swift-algorithms/blob/main/Guides/Trim.md) and
  [Suffix guide](https://github.com/apple/swift-algorithms/blob/main/Guides/Suffix.md)
- Operation: `trimmingSuffix(while:)` discards the trailing elements
  satisfying a predicate and returns the remainder as a non-owning
  subsequence. `suffix(while:)` returns the trailing subsequence up to the
  first element (from the end) that fails the predicate.
- Mapping: `trimmingSuffix(while:)` = drop; `suffix(while:)` = take.
- Implemented today: methods on `BidirectionalCollection` only. The Trim
  guide explains the constraint deliberately: a fallback for a plain
  forward collection would have to traverse the whole collection, and the
  authors chose not to provide a silently inefficient version.
- Adaptor verdict: **the closest existing design template for the C++
  proposal** — bidirectional constraint, backward scan, view-like
  (subsequence) return. Note it lives in Apple's Algorithms package, not in
  Swift's core standard library, which itself only has the front forms.

### 1.3 Haskell — `Data.List.dropWhileEnd`; `Data.Text` / `Data.ByteString` pair — Direct

- Sources: [GHC base documentation for `Data.List`](https://downloads.haskell.org/~ghc/8.6.5/docs/html/libraries/base-4.12.0.0/Data-List.html);
  Hackage documentation for `Data.Text` and `Data.ByteString` (checked in the
  starter research, 15 October 2026).
- Operation: `dropWhileEnd` drops the largest suffix in which the predicate
  holds for all elements. Its own documentation motivates it with
  trailing-whitespace cleanup on text. It has been in `base` since 4.5.0.0.
  `Data.List` has no take counterpart; `Data.Text` and `Data.ByteString` do
  provide both `dropWhileEnd` and `takeWhileEnd`.
- Mapping: drop exact in `base`; the full pair exact in the text/bytes
  libraries.
- Implemented today: in `base`, via a right fold, which preserves useful
  laziness on a forward-only linked list.
- Adaptor verdict: strong, long-standing precedent for the drop side, with a
  caveat a paper should state: Haskell's fold-based version works
  on a forward-only list, whereas an efficient C++ view will require a
  bidirectional range. Swift Algorithms (§1.2) made the same trade-off.

### 1.4 Lodash — `_.dropRightWhile` / `_.takeRightWhile` — Direct

- Source: [Lodash documentation](https://lodash.com/docs/4.18.1)
- Operation: `dropRightWhile` creates a slice excluding elements dropped
  from the end until the predicate returns falsey; `takeRightWhile` is the
  complementary slice taken from the end. Both date to Lodash 3.0.0. The
  documented example drops trailing inactive records from an array of user
  objects — a non-character, record-level use.
- Mapping: exact, both directions ("Right" = last).
- Implemented today: eager functions materialising a new array; the
  predicate may be a function or an iteratee shorthand.
- Adaptor verdict: corroboration of demand and naming convergence in the
  JavaScript ecosystem; like Kotlin, eager where C++ would be lazy.

### 1.5 Ramda — `dropLastWhile` / `takeLastWhile` — Direct

- Source: [Ramda function-selection wiki](https://github.com/ramda/ramda/wiki/What-Function-Should-I-Use)
- Operation: Ramda's own guide lists the pair for lists and strings, with
  the predicate applied to values from the right.
- Mapping: exact, both directions.
- Implemented today: eager/copying library functions.
- Adaptor verdict: further naming/demand corroboration, not a
  standard-library precedent.

### 1.6 Documented gaps — Scala, and Swift core — Demand evidence

- Source: [Scala issue #4247](https://github.com/scala/bug/issues/4247)
- Operation: none — that is the point. The issue notes Scala collections
  have the front and counted forms but no `dropRightWhile` /
  `takeRightWhile`, calls the symmetry incomplete, and a commenter reports
  needing the take form often.
- Adaptor verdict: use only as demand evidence. Do not cite Scala as
  providing these operations. Swift core is in the same position (§1.2).

---

## 2. String-specialised standard-library forms (drop only)

These prove the cleanup use case is ubiquitous, but each is locked to
strings, and usually to a character-set or per-character predicate.

### 2.1 Go — `strings.TrimRightFunc` / `TrimRight` — Direct

- Source: [Go `strings` package documentation](https://pkg.go.dev/strings)
- Operation: `TrimRightFunc` returns the string with all trailing Unicode
  code points satisfying a caller-supplied predicate removed; `TrimRight`
  is the character-set form. The same documentation contrasts these with
  `TrimSuffix`, which removes one provided suffix only.
- Mapping: drop only, arbitrary predicate, strings/runes only. No named
  take counterpart.
- Adaptor verdict: the most predicate-general string precedent. A generic
  C++ adaptor generalises the element type beyond text — trailing NUL,
  sentinel, or default entries in non-string data are the same operation
  and today need a hand-written reverse search.

### 2.2 Rust — `str::trim_end_matches` (char / closure forms) — Direct

- Source: [Rust core documentation for `str`](https://doc.rust-lang.org/1.74.1/core/primitive.str.html)
- Operation: repeatedly removes matching suffix material. When the pattern
  is a `char`, a slice of `char`s, a function, or a closure, this is a
  per-element predicate removal from the end. With a `&str` pattern it is
  repeated whole-pattern removal — a related but different operation, and
  it should not be cited as the predicate form.
- Mapping: drop only, strings only.
- Adaptor verdict: even a systems language ships this only for strings;
  there is no generic iterator/sequence equivalent.

### 2.3 Python — `str.rstrip` — Direct

- Source: [Python standard library documentation](https://docs.python.org/3/library/stdtypes.html)
- Operation: returns a copy with trailing characters removed; the argument
  is a set of characters (all combinations are stripped), defaulting to
  whitespace.
- Mapping: drop only, character-set specialised, strings only, copying.
  `str.removesuffix` is the explicit non-while contrast.
- Adaptor verdict: cannot express a value- or state-dependent predicate at
  all, and nothing for non-string sequences.

### 2.4 Boost String Algorithms — `trim_right_if` — Direct

- Source: [Boost String Algorithms quick reference](https://www.boost.org/doc/libs/1_70_0/doc/html/string_algo/quickref.html)
- Operation: predicate-based trimming from the right of a character
  sequence, with mutating and copy variants. Boost documents its string
  algorithms as generic over character sequences in arbitrary containers.
- Mapping: drop only, character-sequence oriented.
- Adaptor verdict: the nearest C++ precedent, and it shows the gap
  precisely: eager algorithms for character sequences, not lazy adaptors
  over arbitrary element ranges, and no take counterpart.

### 2.5 The C++ ranges plan itself — P2760R1 — Direct

- Source: [P2760R1 ranges plan](https://github.com/brevzin/cpp_proposals/blob/HEAD/2760_ranges_26_plan/ranges-26-plan.md)
- Operation: the plan's table lists both `drop_last_while` and
  `take_last_while` as Tier 1 and notes them as not in range-v3.
- Adaptor verdict: committee-plan context, and direct evidence that the
  incumbent C++ ranges ecosystem does not already supply the pair.

---

## 3. Mature C/C++ codebases — the pattern factored into named helpers

### 3.1 Git — `strbuf_rtrim` and `strbuf_trim_trailing_dir_sep` — Direct (verified in source)

- Source: [git/git `strbuf.c`](https://raw.githubusercontent.com/git/git/master/strbuf.c)
- Operation: `strbuf_rtrim` shrinks a growable string buffer from the end
  while the last character is whitespace, then NUL-terminates.
  `strbuf_trim_trailing_dir_sep` does the same while the last character is
  a directory separator — a path-cleanup use of the identical loop shape.
  A neighbouring helper that strips only one trailing newline (plus an
  optional carriage return) is a fixed-suffix operation and is excluded
  under the precision rule.
- Mapping: drop, with predicates `isspace` and `is_dir_sep` respectively.
- Implemented today: manual reverse loops decrementing the length field,
  factored into named helpers because the pattern recurs across the
  codebase.
- Adaptor verdict: pattern-level evidence (Git is C). In C++ this is the
  canonical `string_view | views::drop_last_while(...)` shape.

### 3.2 LLVM — `StringRef::rtrim` — Direct (verified in source)

- Source: [llvm-project `StringRef.h`](https://raw.githubusercontent.com/llvm/llvm-project/main/llvm/include/llvm/ADT/StringRef.h)
- Operation: returns a shortened, non-owning string view with consecutive
  trailing characters removed — single-character and character-set
  overloads, the set defaulting to whitespace.
- Mapping: drop, predicate = set membership.
- Implemented today: **not** a manual loop — via a find-last-not-of search
  plus dropping from the back, returning a view. This is the closest
  existing C++ analogue to the proposed adaptor: same semantics, eager
  boundary computation, view result.
- Adaptor verdict: strong prior art that the operation wants to be a
  view-producing, predicate-parameterised primitive on a random-access
  range. It would add little inside LLVM itself, which already has the
  helper; its value here is as evidence.

### 3.3 Linux kernel — `strim` — Direct

- Source: [torvalds/linux `lib/string.c`](https://github.com/torvalds/linux/blob/master/lib/string.c)
- Operation: trims a string in place — leading spaces skipped, then an end
  pointer walked backwards over trailing spaces and a NUL written at the
  first of them.
- Mapping: drop (trailing half), plus a leading `drop_while`.
- Implemented today: one central manual helper used kernel-wide.
- Adaptor verdict: pattern-level only (C, in-place). Shows the operation is
  common enough in a very large codebase to warrant a single canonical
  helper.

### 3.4 Chromium — `base::TrimString` / HTTP whitespace trimming — Direct (API and call sites) / partial

- Source: [Chromium code review — `base/strings/string_util.h`](https://codereview.chromium.org/1169393003/diff/160001/base/strings/string_util.h)
- Operation: a trim API with a trailing-only mode and a caller-specified
  character set, used among other places to strip trailing HTTP linear
  whitespace from header values.
- Mapping: drop, predicate = set membership.
- Implemented today: library helper. The API, mode flag, and call sites
  were directly evidenced; the current implementation body in modern
  Chromium was not inspected in this pass, so no implementation style is
  claimed beyond "library helper".
- Adaptor verdict: uniformity value — every mature stack has this helper
  under its own name.

---

## 4. Text processing, parsers, and protocol specifications

### 4.1 WHATWG URL Standard — trailing C0 control / space removal — Direct (verified in spec)

- Source: [WHATWG URL Living Standard](https://url.spec.whatwg.org/)
- Operation: the basic URL parser's first steps flag leading or trailing
  C0 control or space as a validation error and then remove any leading
  and trailing C0 control or space from the input (tab and newline are
  separately removed throughout).
- Mapping: the trailing half is exactly drop over the character sequence,
  with predicate "is C0 control or space".
- Implemented today: a specification-level algorithm; browsers implement
  it as index-trimming over a mutable string.
- Adaptor verdict: low–moderate. A view-based trim states the same shape,
  but the value is expressive rather than enabling.

### 4.2 HTTP field values — trailing optional whitespace — Direct

- Sources: [RFC 9110 §5.5](https://www.rfc-editor.org/rfc/rfc9110.html);
  [aiohttp pull request fixing a parser divergence](https://github.com/aio-libs/aiohttp/pull/13246)
- Operation: RFC 9110 provides that a field value does not include leading
  or trailing whitespace and that a parser must exclude it before
  evaluating the value. The aiohttp change records a real divergence bug:
  one parser path kept trailing space/tab in header values while another
  stripped them, and the fix applied the same strip in both.
- Mapping: drop, predicate = space or horizontal tab, over the value's
  characters (plus the leading mirror).
- Implemented today: a bespoke trim helper in every HTTP stack.
- Adaptor verdict: moderate. This is the same two-ended trim re-implemented
  per stack; the divergence bug is useful evidence that hand-rolled copies
  of this operation drift.

### 4.3 Git — `git stripspace` — Direct (verified in official docs)

- Source: [git-stripspace documentation](https://git-scm.com/docs/git-stripspace)
- Operation: the documented cleanup for commit messages, notes, and tags
  removes trailing whitespace from every line, collapses runs of empty
  lines, removes empty lines from the beginning **and end** of the input,
  and ensures a final newline.
- Mapping: two nested instances — over characters per line, drop with
  predicate "is space"; over the **sequence of lines**, drop with
  predicate "line is empty". The second is the cleanest sequence-level
  (non-character) example in this research.
- Implemented today: a streaming C filter in Git core.
- Adaptor verdict: moderate–high at the line level. "Drop trailing empty
  lines from a range of lines" is a textbook use over a range of ranges.

### 4.4 FFmpeg — `ff_get_chomp_line` and the LRC encoder — Direct

- Sources: [FFmpeg patch showing the chomp-line implementation](https://patchwork.ffmpeg.org/project/ffmpeg/patch/20170505165045.4154-1-lq@chinaffmpeg.org/);
  [FFmpeg LRC encoder source documentation](https://ffmpeg.org/doxygen/7.0/lrcenc_8c_source.html)
- Operation: a line-reading helper documented as behaving like the plain
  line reader but stripping whitespace characters from the tail of the
  text; the implementation decrements the length while the last byte is
  in a space-character set and NUL-terminates. Separately, the LRC
  subtitle encoder walks an end pointer back over trailing CR/LF
  characters.
- Mapping: drop, predicates "is space character" and "is CR or LF".
- Implemented today: manual backward loops in C; the helper exists
  because line-tail stripping recurs across demuxers.
- Adaptor verdict: pattern-level only (C). Confirms protocol/format line
  parsing as a major habitat for the operation.

### 4.5 Linux kernel — `load_script` (`#!` line parsing) — Direct

- Source: [LKML patch thread for `fs/binfmt_script.c`](https://lkml.indiana.edu/hypermail/linux/kernel/2106.2/06129.html)
- Operation: when parsing a `#!` interpreter line, trailing spaces and
  tabs are trimmed from the end of the line by decrementing the end
  pointer while the preceding character is a space or tab.
- Mapping: drop, predicate = space-or-tab.
- Implemented today: a manual loop inline at the parse site, with no
  helper — the pattern reinvented at a call site.
- Adaptor verdict: pattern-level only (C), but a good "ad hoc, un-helpered"
  instance.

### 4.6 YAML block scalars — chomping — Direct (spec), partial mapping

- Source: [YAML 1.2.2 specification, block chomping](https://yaml.org/spec/1.2.2/#8112-block-chomping-indicator)
- Operation: a chomping indicator controls a block scalar's trailing line
  breaks: strip removes every trailing line break; keep preserves all;
  clip (the default) keeps exactly one final line break.
- Mapping: strip is drop with predicate "is line break". Clip is **not** a
  pure drop — it drops the trailing blank-line run and then retains one
  final break.
- Adaptor verdict: low as a use site (the decision is made mid-scan, with
  end-of-input interactions), but useful as evidence that trailing-run
  handling is specification-critical in a widely used format. Present clip
  honestly as a near-miss, not an example.

---

## 5. Paths, filenames, and filesystem formats

### 5.1 Go — `path/filepath.Base` — Direct

- Source: [Go standard library, `path/filepath`](https://pkg.go.dev/path/filepath)
- Operation: to produce the final element of a path, the implementation
  first shrinks the path from the end while the last element is a path
  separator, then scans backwards to the previous separator and returns
  the final element.
- Mapping: a composition — drop with predicate "is path separator",
  followed in effect by take with the negated predicate ("is not a
  separator") to isolate the final segment.
- Adaptor verdict: a real algorithm chaining a drop-last and a take-last.
  It also surfaces a design point: the second step uses the negated
  predicate, i.e. "final segment" semantics.

### 5.2 Linux kernel, exFAT — trailing periods in pathnames — Direct

- Sources: [kernel patch discussion](https://lists.openwall.net/linux-kernel/2022/03/17/483);
  [linux-fsdevel discussion](https://www.spinics.net/lists/linux-fsdevel/msg196101.html)
- Operation: a helper returns the length of a pathname ignoring all
  trailing `.` characters, decrementing the length while the last
  character is a period. Windows/FAT-family semantics treat trailing
  periods as insignificant; later discussion made the stripping optional
  after reports that it silently resolved one name to a different file.
- Mapping: drop, predicate = `c == '.'`, returning the kept length rather
  than mutating.
- Adaptor verdict: pattern-level only (C). Also a cautionary tale worth
  one line in a paper: whether dropping a trailing run is *correct* can
  be observable and controversial, independently of how it is spelled.

### 5.3 Git — trailing directory separators — Direct

See §3.1: `strbuf_trim_trailing_dir_sep` is the path instance of Git's
helper family.

### 5.4 fileseq (VFX image sequences) — trailing frame-number digits — Direct

- Sources: [fileseq issue containing the on-disk matching pattern](https://github.com/justinfx/fileseq/issues/81);
  [fileseq changelog](https://github.com/justinfx/fileseq/blob/HEAD/CHANGES.md)
- Operation: files on disk are grouped into sequences by a pattern that
  anchors an optional, variable-length group of digits at the end of the
  filename stem, before an optional extension — so a name is split into
  directory, basename, frame-number run, and extension.
- Mapping: take, predicate = "is digit", over the filename stem.
- Implemented today: historically a notoriously fiddly regular expression;
  the project's changelog records that version 3.0.0 replaced the regexes
  with a shared grammar — the regex form was abandoned as
  unmaintainable.
- Adaptor verdict: moderate. In imperative C++ tooling that scans
  directory entries, taking the trailing digit run (and its complement)
  states the split directly and avoids the nested-optional regex. This is
  the canonical shape of take-side demand for filenames.

### 5.5 Blender — `BLI_path_frame_get` — Direct (behaviour) / inferred (implementation)

- Sources: [Blender fix commit](https://github.com/bb-yi/blender/commit/07211674122eb66f7750bbd9e1325c50557f6bc4);
  [upstream Blender pull request](https://projects.blender.org/blender/blender/pulls/150917)
- Operation: extracts a frame number from a path and reports the length
  of the trailing digit run alongside it; the fix commit evidences that
  contract (an early return previously failed to set the digit length,
  causing a crash).
- Mapping: take, predicate = "is digit", over the stem — with the run's
  length as a first-class output.
- Implemented today: hand-written C++ scanning; **the scanning loop itself
  was not inspected** in this research, so the implementation mechanism
  is inferred, not verified.
- Adaptor verdict: moderate, conditional on that inference: the API shape
  (value plus digit-run length) is precisely what a take-side adaptor
  yields.

### 5.6 RapidRAW — filename-number rename token — Direct

- Source: [RapidRAW commit adding the token](https://github.com/cybertimon/rapidraw/commit/c513bdb4bde935a00442d03eeb77bbfc3ae71c25)
- Operation: a rename/import/export token defined as the trailing digits
  of the file stem (leading zeros preserved; empty if the stem does not
  end in digits).
- Mapping: take, predicate = "is ASCII digit", over the stem.
- Adaptor verdict: the result must remain a string/view, not an integer —
  leading zeros are data (camera frame numbers). A view adaptor preserves
  exactly that; an integer parse would not.

### 5.7 FrameStack — trailing frame number vs first digit run — Direct

- Source: [FrameStack audit document](https://github.com/sousaplex/framestack/blob/HEAD/docs/AUDIT.md)
- Operation: the project's own audit flags sequence detection that used
  the *first* digit run in a basename as a bug for names containing an
  earlier version number, and prescribes matching the *trailing* number
  group for both pattern replacement and sort key.
- Mapping: take, predicate = "is digit", over the stem — explicitly the
  last-run variant, contrasted with the front-run variant as a live
  defect.
- Adaptor verdict: direct evidence that the *last*-run variant is the one
  real code needs here.

---

## 6. Binary formats and padding — mostly spec-level

This category is weaker than the others, and a paper should not overclaim
it.

### 6.1 exFAT / FAT-family pathname semantics — Direct

See §5.2. This is the one format-driven case in this research with an
inspected looping implementation.

### 6.2 Fixed-width padded fields — Spec-only / inferred

- Sources: [PE/COFF specification (via Manalyze's copy)](https://mygithub.libinneed.workers.dev/JusticeRage/Manalyze/raw/refs/heads/master/resources/pecoff.pdf);
  [libarchive `tar.5` documentation](https://sources.debian.org/src/libarchive/3.6.2-1%2Bdeb12u4/doc/pdf/tar.5.pdf);
  [FAT documentation](https://en.wikipedia.org/wiki/FAT_ACCDATE)
- Operation: format specifications establish trailing padding runs —
  space-padded 8.3 name fields in FAT directory entries, a NUL-padded
  8-byte section name field in PE/COFF, NUL-filled fields in tar headers.
  Reading such a field requires discarding the trailing padding run
  (spaces or NUL bytes respectively).
- Mapping: drop, predicate = "is space" or "is NUL" — **inferred from the
  format specifications. No looping strip implementation for these fields
  was inspected in this research.**
- Adaptor verdict: do not present these as code examples. They establish
  only that trailing padding runs are pervasive in binary formats; the
  code-level evidence for the drop operation comes from §§3–5 and 7.

### 6.3 Trailing NUL bytes and zero padding in buffers — gap

No directly evidenced, named, looping implementation for stripping a
trailing run of NUL bytes or zero padding from a general buffer was found
in the codebases searched in this pass. Fixed-size field handling (§6.2)
and the character-set trims of §3 cover adjacent ground, but a paper
should not claim a specific trailing-NUL code example on this evidence.

---

## 7. Numeric representation and formatting — trailing zeros

The recurring caveat for this whole section: in production numeric code
the run operation is usually inseparable from exponent/scale adjustment,
an all-matches policy, a stop boundary (decimal point or exponent), and
significance (trailing zeros often *mean* precision). A generic adaptor
expresses the run-selection core; the domain invariants remain the
caller's. Several cases below make exactly that point by deliberately
*not* stripping.

### 7.1 Go — `math/big` decimal `trim` — Direct (verified in source)

- Source: [Go source, `math/big/decimal.go`](https://go.dev/src/math/big/decimal.go)
- Operation: a `trim` helper cuts trailing zeros from a decimal mantissa
  (a backward scan over the digit bytes, then a reslice); if every digit
  is removed, the exponent is reset to 0. The same file trims trailing
  zeros inline when initialising a decimal, noting that the exponent
  tracks the decimal point independently of the digit count.
- Mapping: drop, predicate = digit is `'0'`, over the mantissa digits.
- Adaptor verdict: literal drop on the digit buffer, with the honest
  caveat that value preservation requires the exponent bookkeeping the
  semantic code does alongside it.

### 7.2 Go — `strconv` Ryu shortest-formatting path — Direct

- Source: [Go source, `strconv/ftoaryu.go`](https://go.googlesource.com/go/+/refs/tags/go1.25.0/src/strconv/ftoaryu.go)
- Operation: after rendering a fixed-precision decimal significand, the
  digit count is shrunk while the last digit is `'0'`, the number removed
  is counted, and that count is added to the decimal-point position.
- Mapping: drop, predicate = digit is `'0'`, on the digit buffer.
- Adaptor verdict: as §7.1 — the adaptor alone would be wrong without the
  point-position compensation.

### 7.3 Python `packaging` (PEP 440) — trailing zero release components — Direct (verified in source) ★

- Source: [pypa/packaging `version.py`](https://github.com/pypa/packaging/blob/main/src/packaging/version.py)
- Operation: before version comparison, trailing zeros are stripped from
  the release tuple so that, for example, `1.0.0` and `1` compare equal.
  The current code does this with an explicit backward loop over the
  tuple, under a comment stating the purpose. (An earlier vendored copy
  of the same library achieved it by reversing, applying a front
  drop-while, and reversing back — the contortion a developer reaches
  for when no end-form exists.)
- Mapping: drop, predicate = component equals 0, over a sequence of
  integers — **not** characters.
- Adaptor verdict: **one of the strongest exhibits in this file.** It is
  production code, in a library on the critical path of Python packaging,
  performing exactly this operation on a non-text sequence, with a
  comment explaining why. A named end-form collapses it to one operation.

### 7.4 Java — `BigDecimal.stripTrailingZeros()` — Direct (API) / inferred (adaptor mapping)

- Source: [Java SE documentation for `BigDecimal`](https://docs.oracle.com/en/java/javase/17/docs/api/java.base/java/math/BigDecimal.html)
- Operation: returns a numerically equal value with trailing zeros
  removed from the representation, operating on the unscaled-value/scale
  pair; zero is returned as a canonical zero value.
- Mapping: drop over the unscaled digit sequence, conceptually — a
  semantic API, not string code.
- Adaptor verdict: also a counterweight. Stripping can produce scientific
  notation, and scale is semantically significant in Java (values with
  different scales can be numerically equal but not equal as objects), so
  a raw string-level drop would be wrong. Evidence that the *operation*
  is standard-worthy, and that domain semantics sit outside it.

### 7.5 Python — `Decimal.normalize()` — Direct

- Source: [Python `decimal` documentation](https://docs.python.org/3/library/decimal.html)
- Operation: reduces a finite value to its simplest canonical form by
  repeatedly removing a trailing zero from the coefficient and adjusting
  the exponent, after applying context rounding.
- Mapping: drop over the coefficient digits, with exponent compensation.
- Adaptor verdict: as §7.4 — a semantic API; also note the module
  elsewhere *preserves* significant trailing zeros (for example in
  monetary addition), which is precisely why this must be an opt-in
  operation rather than a formatting default.

### 7.6 `%g`-family formatting — Direct (documented behaviour)

- Sources: [Go `fmt` documentation source](https://github.com/golang/go/blob/master/src/fmt/doc.go);
  [.NET standard numeric format strings](https://github.com/dotnet/docs/blob/main/docs/standard/base-types/standard-numeric-format-strings.md);
  [Abseil `numbers.h`](https://github.com/abseil/abseil-cpp/blob/master/absl/strings/numbers.h)
- Operation: the general format removes trailing zeros after the decimal
  point (with an opt-out: the alternate form in C/Go preserves them;
  .NET's `Decimal` with omitted precision is a documented exception).
- Mapping: drop over the fractional digits, followed by conditional
  removal of a now-trailing decimal point — a two-step operation bounded
  by the point. A whole-string drop would also eat an exponent or integer
  zeros.
- Adaptor verdict: output-level evidence that trailing-zero removal is a
  default expectation in general numeric formatting, and a worked example
  of why the operation must be applied to the right subrange.

### 7.7 NumPy — `trim_zeros(..., trim='b')` — Direct (verified in docs)

- Source: [NumPy `trim_zeros` reference](https://numpy.org/doc/stable/reference/generated/numpy.trim_zeros.html)
- Operation: trims zeros from the front, the back, or both ends of an
  array or sequence; the back form is the one at issue. Current versions
  generalise to axes / bounding-box cropping.
- Mapping: drop, predicate = element equals 0, on a **numeric** range —
  the cleanest non-string analogue in this section.
- Adaptor verdict: a dedicated library function exists because users need
  this on arrays; a generic adaptor covers the one-dimensional case with
  any predicate (for example a tolerance), which the fixed function does
  not.

### 7.8 NumPy polynomial — `polytrim` / `chebtrim` — Direct

- Source: [NumPy polynomial routines documentation](https://docs.scipy.org/doc//numpy-1.9.0/reference/routines.polynomials.polynomial.html)
- Operation: removes small trailing coefficients from a polynomial —
  "trailing" meaning highest-order, since coefficients are stored
  lowest-order first; if everything is trimmed, a single zero is
  returned. The Chebyshev variant's predicate is tolerance-based rather
  than equality with zero.
- Mapping: drop, predicate = coefficient is small (by tolerance).
- Adaptor verdict: generalises the story usefully — the predicate can be
  approximate — and shows the limit: the empty-result convention (a
  single zero, not an empty range) is domain policy the adaptor does not
  supply.

### 7.9 PostGIS's vendored Ryu — buffer-level trailing-zero strip — Direct

- Sources: [PostGIS vendored Ryu readme/history](https://github.com/postgis/postgis/blob/HEAD/deps/ryu/README.md);
  [upstream Ryu issue #142](https://github.com/ulfjack/ryu/issues/142)
- Operation: upstream Ryu's shortest mode produces no trailing zeros by
  construction, and its fixed/exponential modes emit them deliberately
  because precision was requested. PostGIS vendored Ryu with changes to
  remove trailing zeros for its double-printing: its code walks the
  output buffer back over `'0'` characters, then over a trailing decimal
  point, guarded by a compile-time flag.
- Mapping: drop over the output buffer, followed by a single-element
  drop of the point.
- Adaptor verdict: a fork had to be maintained to get this behaviour —
  demand evidence — and the guards around it (stop before an exponent,
  only when a point was printed) restate §7's caveat.

### 7.10 Negative finding — {fmt} and double-conversion — Direct (for the position stated)

- Source: [{fmt} issue #1551](https://github.com/fmtlib/fmt/issues/1551)
- Operation: none in-library. A user asking for fixed-precision output
  without trailing zeros was told it can be done as post-processing and
  need not be in the core; high-performance formatters generally prefer
  *generating* no trailing zeros (shortest-digit algorithms) over
  generating and then dropping, and treat fixed-precision zeros as
  requested data.
- Adaptor verdict: cite only as that construction argument. Do not cite
  {fmt} or double-conversion as containing a trailing-zero strip; none
  was found.

---

## 8. Records, logs, configuration, and token streams

### 8.1 Java — `String.split` with limit 0 discards trailing empty fields — Direct (verified in docs) ★

- Source: [Java SE documentation for `String`](https://docs.oracle.com/en/java/javase/17/docs/api/java.base/java/lang/String.html)
- Operation: the one-argument split behaves as if the limit were 0, and
  with limit 0 trailing empty strings are not included in the result
  array.
- Mapping: drop over the **result sequence of fields**, predicate =
  field is empty. This is a drop over a sequence of substrings, not
  characters — directly relevant to delimited records and log lines,
  where a record ending in empty fields loses them by default.
- Adaptor verdict: low for Java itself (the behaviour is frozen into a
  platform API, and its surprise factor — fields silently vanishing — is
  a known footgun), but high as design evidence: the drop is exactly the
  semantic a platform chose as the default for splitting records, and in
  a ranges world it should be an explicit, named, controllable operation
  rather than an implicit side effect of a limit parameter.

### 8.2 Android AIDL — trailing blank comment lines — Direct

- Source: [AIDL `comments.cpp` (Android platform mirror)](https://fuchsia.googlesource.com/third_party/android/platform/system/tools/aidl/+/refs/tags/android-vts-14.0_r1/comments.cpp)
- Operation: given a vector of comment lines, repeatedly pop from the
  back while the last line, once trimmed, is empty.
- Mapping: drop over a container of strings, predicate = line is blank.
  Proof the pattern is not character-specific.
- Adaptor verdict: **an honest counter-example for a lazy view.** The
  caller owns the container and wants in-place mutation; an eager
  algorithm or the loop fits better than a view. Worth one sentence in a
  paper's "when not to use" discussion — and it argues for considering
  whether an algorithm form is also needed.

### 8.3 Trailing empty lines in text records — Direct

See §4.3 (`git stripspace`): removing empty lines from the end of the
input is the same operation over a range of lines, in a tool whose entire
purpose is cleaning text records (commit messages, notes, tags).

### 8.4 Configuration and data cleanup — summary of evidence

No separate, primary-source configuration-file example was found in this
pass beyond the text/record cases above (§§4.3, 8.1, 8.2) and the version
normalisation of §7.3, which is data cleanup over a tuple of integers.
Searches aimed specifically at configuration parsing (including Git's
config parser) produced no qualifying trailing-run operation. A paper
should claim configuration/data cleanup only to the extent of those
examples, not as an independently evidenced category.

### 8.5 Token streams — honest gap

No directly evidenced token-stream example was found. Lexers discard
whitespace and comments *throughout* a stream (a per-token skip — repeated
front drop-while before each token), which is a different operation from
removing a trailing run at end-of-stream. No source inspected in this pass
performs a trailing-run drop or take over a token sequence. This category
should be omitted from a paper's motivation, or mentioned only as
searched-and-not-found.

---

## 9. The take side, collected

The drop side dominates in the wild. Standalone named take helpers were
found only in the language/library precedents of §1 (Kotlin, Lodash,
Ramda, Swift Algorithms, Haskell's text/bytes libraries). In production
codebases the take side appears unnamed, in two shapes:

### 9.1 Measuring the trailing run — Git `trailing_spaces_len` — Direct

- Sources: [Git mailing-list patch](https://www.spinics.net/lists/git/msg135860.html);
  [commit mirror](https://gitlab.utu.fi/taahol/git/-/commit/5a12c8864b9485351547a45fc759dde30dd1d870)
- Operation: walk backwards from the end of a patch-header line counting
  the run of trailing space characters and return its length; the caller
  subtracts it to recover the filename/timestamp boundary in traditional
  diff headers.
- Mapping: the size of take, predicate = character is a space — computed
  in service of an implicit drop.
- Adaptor verdict: the best codebase evidence that the take side is
  independently wanted: here only the trailing run itself (its length)
  matters. On a random-access range this is a take view plus a distance.

### 9.2 Extracting a trailing component — paths and filenames

- Go `path/filepath.Base` (§5.1): take with a negated predicate isolates
  the final path segment.
- fileseq (§5.4), Blender (§5.5), RapidRAW (§5.6), FrameStack (§5.7):
  take with predicate "is digit" extracts the trailing frame-number run
  from a filename stem.

### 9.3 Go compiler — `//line` directive trailing digits — Direct (operation) / inferred (mapping)

- Source: [Go compiler syntax parser source](https://go.dev/src/cmd/compile/internal/syntax/parser.go?m=text)
- Operation: parsing `filename:line[:col]` directives obtains the
  trailing digit run after the last colon, validates it as an unsigned
  integer in range, and removes `:line` / `:col` from the filename.
- Mapping: the kept suffix is the trailing digit run in intent, and the
  remaining filename is its complement — but the implementation finds the
  last colon and parses the whole remainder; it is not a digit scan, so
  the mapping is inferred.
- Adaptor verdict: a model caveat. The semantic version must also handle
  Windows paths containing colons, a non-numeric suffix as an error
  rather than an empty take, zero/overflow line numbers, and an empty
  filename meaning "previous file". A raw adaptor supplies only the run;
  all validation still follows.

### 9.4 The asymmetry is itself a finding

The drop side has converged on named helpers everywhere it occurs
(§§2–3). The take side, outside §1's libraries, is still hand-rolled
everywhere it occurs. That asymmetry supports proposing the pair together,
mirroring the existing front pair (`drop_while` / `take_while`) and the
counted back pair (`drop_last` / `take_last`, P4294).

---

## 10. Synthesis — the strongest cases for a WG21 paper

Ranked by strength of evidence and directness of mapping:

1. **The exact pair already exists in mainstream libraries, always eager
   or non-core.** Kotlin's standard library ships both operations with
   the proposed naming (§1.1); Lodash and Ramda ship both (§§1.4–1.5);
   Apple's Algorithms package ships both with the design the C++
   proposal needs — bidirectional constraint, backward scan, non-owning
   return (§1.2); Haskell has shipped the drop form since 2012 and the
   pair for text/bytes (§1.3). No source found provides a lazy, generic,
   bidirectional-range view of either operation. That unoccupied
   combination is the paper's central point.
2. **`packaging._cmpkey` (§7.3)** — production code on Python's packaging
   critical path stripping a trailing run of zero *integers* (not
   characters), with an explanatory comment, in current code by an
   explicit backward loop and in an earlier version by a
   reverse/drop-while/reverse contortion.
3. **Java `String.split` (§8.1)** — a platform-default drop of trailing
   empty *fields* in record splitting, over a sequence of substrings.
   Shows the operation acting on token/field sequences in everyday data
   processing, and that making it implicit causes silent data loss —
   the argument for an explicit named operation.
4. **The named-helper convergence in C/C++ (§§3.1–3.3)** — Git's
   `strbuf_rtrim` / `strbuf_trim_trailing_dir_sep`, the Linux kernel's
   `strim`, and LLVM's `StringRef::rtrim` are the same operation,
   independently factored into helpers in three of the largest C/C++
   codebases in existence. LLVM's version — a find-last-not-of plus a
   view return — is effectively the proposed adaptor, hand-built for
   `char` only.
5. **Specification-level demand (§§4.1–4.2)** — the WHATWG URL parser and
   HTTP field-value parsing both mandate trailing-whitespace removal;
   the aiohttp divergence bug shows hand-rolled copies of it drifting
   apart in production.
6. **The take side's filename habitat (§§5.4–5.7, 9.1)** — trailing
   digit-run extraction for image-sequence frame numbers is a repeated,
   independently implemented need (fileseq, Blender, RapidRAW,
   FrameStack), including one project's audit identifying use of the
   *front* run instead of the trailing run as a bug.

### What the evidence does not support — do not claim

- Frequency counts. This catalogue establishes breadth and
  convergence, not counts: it produced no frequency numbers of its
  own, and none of its entries should be read as one. (Counts do
  exist, from the separate counting passes in `frequency-study.md`
  and `use-case-research-deep.md`; the paper quotes those, not
  anything from this file.)
- Token streams (§8.5): searched, not found.
- Binary-format code examples (§6.2–6.3): padding runs are spec-evidenced
  only; the one format-driven code case is exFAT (§5.2).
- Fixed-suffix removals are not evidence for a while operation, however
  common they are.
- A streaming/lazy-filter framing: the operation is end-anchored and the
  precedents are bidirectional/materialised (§0). The claim is a
  lazy *view* over a bidirectional range whose boundary is found by a
  backward scan — not single-pass laziness.
- Universal improvement: where the caller owns a container and wants
  mutation (§8.2), or where domain invariants ride along with the run
  (§7, especially 7.4 and 9.3), a view adaptor is only the run-selection
  core of the solution, and a paper should say so.

### One-sentence summary for the paper's motivation section

Removing (or isolating) a maximal trailing run by predicate is already a
named standard-library operation in Kotlin, Haskell, Lodash, Ramda, and
Apple's Algorithms package, a named helper in Git, LLVM, and the Linux
kernel, a mandated step in the URL and HTTP specifications, and a
hand-rolled backward loop in numeric formatting and version handling —
in C++ it is none of those things for a general range, which is the gap
P2760 Tier 1 identifies.
