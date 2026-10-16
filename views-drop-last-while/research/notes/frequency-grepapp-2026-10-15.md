# grep.app live counts — verified live 2026-10-15 (BST evening)
Index: grep.app (Vercel) — public GitHub repos indexed; unit shown as "results found" = files containing >=1 match (per grep.app UI; individual files show N matches separately). Not deduplicated for vendored copies, docs, tests, definitions. Language facets rounded (1k, 2k, 7k, 30k).

- dropLastWhile: 1,744 results. Lang: Kotlin ~1k, JS 248, MD 49, TS 38, PHP 24. Top repos flow-typed 39, JetBrains/kotlin 34. Also Ramda, Remeda, Deno std drop_last_while.ts, Go samber/lo DropLastWhile. URL https://grep.app/search?q=dropLastWhile
- takeLastWhile: 623 results. JS 232, Kotlin 188. URL https://grep.app/search?q=takeLastWhile
- dropWhileEnd: 626 results. Haskell 562. Top ghc/ghc 27, pandoc 13. URL https://grep.app/search?q=dropWhileEnd
- takeWhileEnd: 152 results. Haskell 138. NOTE: NOT in Haskell base Data.List (only dropWhileEnd there); in Data.Text, Data.ByteString, Data.List.Extra. Pandoc uses T.takeWhileEnd (/=':') and (/='\n'). URL https://grep.app/search?q=takeWhileEnd
- dropRightWhile: 3,138 results. JS ~2k. Path facets show public/ 463, node_modules/ 234, dist/ 185, docs/ 181 — vendored Lodash inflation. URL https://grep.app/search?q=dropRightWhile
- takeRightWhile: 3,024 results. Similar vendoring: public/ 462, node_modules/ 234. URL https://grep.app/search?q=takeRightWhile
- find_last_not_of (C++ string analogue, indirect): 7,928 results, C++ ~7k. Includes declarations in libc++/std headers, tests. URL https://grep.app/search?q=find_last_not_of
- trim_end (indirect, noisy): 35,018 results, Rust ~30k; includes unrelated identifiers e.g. trim_end_ms. Label APPROXIMATE/indirect only. URL https://grep.app/search?q=trim_end

Cross-check blockers (2026-10-15, verified live):
- GitHub Code Search https://github.com/search?q=dropLastWhile&type=code — requires sign-in; no count obtainable signed-out. Do NOT present a number.
- Sourcegraph https://sourcegraph.com/search?q=context:global+dropLastWhile&patternType=keyword — Cloudflare bot verification page; not bypassed.
- searchcode.com classic code index no longer at /?q= — site repurposed to per-repo dossier/MCP; no global count obtained.
=> Cross-index exact replication NOT achieved; grep.app is single-index VERIFIED-live for its own universe, others NOT RELIABLY MEASURABLE signed-out.

Concrete hits seen in grep.app results (use cases):
- Kotlin split-then-dropLastWhile {it.isEmpty()} ubiquitous: SmartDock Utils.kt calculator split, Pocket Casts IntentUtil.kt URL split "&", EhViewer cookieStrings split ";"
- Sentry (getsentry/sentry) useQueryBasedColumnResize.tsx: dropRightWhile(widths, width => width === COL_WIDTH_UNDEFINED) — trailing undefined column widths, drop_last_while
- GHC filepath: dropWhileEnd isPathSeparator for trailing path separator; dropWhileEnd (==' ') on filenames
- WAI (yesodweb/wai) Network/Wai/Util.hs trimWS = dropWhileEnd (== space)
- Cabal dropWhileEndLE/takeWhileEndLE comments: specification reverse.dropWhile.reverse but faster / easier to read
