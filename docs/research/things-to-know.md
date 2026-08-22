# Things to know before building

Research findings, prior art, and known pitfalls gathered before implementation
started. Every item here is a claim someone already paid for — a shipped bug, a
benchmark, or a documented failure mode in an existing tool. Read the section
that covers what you are about to build.

**As of 2026-08.** Date any claim you add; delete one that goes stale rather than
letting it rot.

## 1. Transport and protocol-level gotchas

- **stdio is the near-universal transport in production.** TCP/socket exists in the spec mainly for debugging (attach to a standalone server) or browser-based editors. Every major editor (VS Code, Neovim, Sublime, JetBrains via LSP4IJ) and Claude Code's own LSP integrations spawn the server as a child process over stdio by default. Build stdio as the only required transport.

- **Position encoding is a well-known, long-running source of bugs across the entire LSP ecosystem.** The spec historically mandated UTF-16 code units for line/character offsets in `Position`, a legacy of VS Code being JavaScript-based, even though the document content itself is transmitted as UTF-8 and most non-JS servers store text internally as UTF-8 or UTF-32 codepoints. This mismatch has caused real, shipped bugs — for example, certain multibyte emoji sequences have caused rust-analyzer to panic because it computed an offset that landed in the middle of a multi-code-unit character. LSP 3.17 added a negotiable `positionEncoding` capability (`utf-8`, `utf-16`, `utf-32`), but UTF-16 must still always be supported as the fallback default. **Implication**: the engine must implement UTF-8 ↔ UTF-16 ↔ codepoint offset conversion correctly and test it explicitly against multi-byte and surrogate-pair (astral-plane, e.g. emoji) characters from day one, negotiate `positionEncoding` at `initialize` time, and prefer UTF-8 or UTF-32 internally when the client supports it, converting only at the protocol boundary.

- **Line-ending ambiguity**: the spec recognizes `\n`, `\r\n`, and `\r` as valid end-of-line sequences, and a `Position` cannot legally point inside a line-ending sequence. Line-splitting logic must treat these three forms consistently and identically on both hypothetical client and server sides — since this engine has no real client-side counterpart, the important part is being consistent about which EOL convention a file uses internally in the cache.

- **Multi-root workspaces are a first-class native LSP capability** (`workspaceFolders` in `initialize`), not something to bolt on. A single spawned server process can and should be told about multiple project roots up front and manage them internally — this is directly relevant to the daemon design, since even the "one process per invocation" baseline model already assumes multi-root support in the spec.

## 2. Incremental parsing pitfalls (tree-sitter specifically)

- **Incremental reparse is not guaranteed to produce the same tree as a fresh parse of the same final text.** Confirmed, reported behavior: applying an edit incrementally can leave spurious error nodes in the tree that a from-scratch parse of the identical resulting text would not produce. This is a known category of bug, not a hypothetical edge case. **Implication**: the engine should periodically perform a full fresh reparse as a correctness safety net (e.g. on save, or after N consecutive incremental edits, or when an incrementally-produced tree contains unexpected error nodes), rather than trusting incremental application indefinitely.

- **Error recovery behavior is effectively a black box** the grammar/engine author has limited ability to tune — there's no way to hint at the "cost" of a skipped or missing token, so in rare cases the parser can make a poor recovery choice for malformed/incomplete code, and there's little recourse besides accepting the result. This matters especially for the interactive-typing case, where the code is transiently invalid on almost every keystroke.

- **Only named/visible nodes are reliably walkable**; explicitly `MISSING` nodes (synthesized by error recovery) can be hard to distinguish or introspect, which complicates building a compiler-quality error message from a tree-sitter error node versus using it just for "something is wrong here" diagnostics. Tree-sitter is well-suited to Tier 1 (syntax highlighting, folding, structure) and reasonably suited to driving Tier 2 (scope/binding), but is not by itself a substitute for a real diagnostic-quality error reporter — the type/binding layers need to generate their own precise error messages rather than relying on tree-sitter's own error nodes for anything beyond "mark this range as problematic."

- Tree-sitter's runtime library itself is plain C, which fits directly into this project's dependency profile regardless of whether the rest of the engine is written in C or C++.

## 3. Type inference / constraint-solving (Tier 3) pitfalls

- **Union-find/unification is the right core algorithm** for the equality-constraint part of type solving (this matches the earlier design decision) — it's the standard technique behind essentially every Hindley-Milner-style inference implementation (the "Algorithm W" / constraint-generation-then-unification split is the most common formulation, and is friendlier to an incremental, non-strictly-left-to-right engine than the classical substitution-heavy formulation, since constraints can be collected and solved somewhat independently of generation order).

- **The occurs check is a mandatory step, not optional.** Unifying a type variable with a type that recursively contains itself (e.g. attempting `T = list(T)`) must be explicitly detected and rejected as an infinite type, or the solver can recurse indefinitely / build unbounded structures. Every real implementation includes this check; omitting it is a common first-implementation bug.

- **Pathological complexity cases exist even in "simple" Hindley-Milner-style inference** — certain constructs (notably tuple/record-heavy code, and in general with enough deeply nested polymorphic let-bindings) can hit worst-case exponential-time behavior in naive implementations. Worth budgeting for a per-file or per-expression solving time/step limit that degrades to "unknown type" rather than allowing pathological input to hang the interactive worker thread.

- **Type errors benefit from a "type graph," not just a substitution/unification trace**, if the goal is genuinely useful diagnostics (as opposed to just "yes/no, type checks"). Constraint-based formulations that retain the graph of *why* a constraint was generated (not just the final substitution) allow producing an explanation like "this call requires X because of this earlier argument," which is significantly more useful to a human or agent than a bare mismatch error. Worth designing the constraint representation with this traceability in mind from the start, since retrofitting it later is expensive.

- Real-world implementers (e.g. Rust's rustc, various ML/Haskell-family compilers) treat generalization/instantiation of polymorphic types, and level/scope-aware unification (so a type variable doesn't escape the let-binding scope it was introduced in) as core, non-optional parts of a usable inference engine, not advanced extensions — worth scoping the initial Tier 3 design around this from the start rather than treating it as a later add-on, since it affects the fundamental data representation of a "type variable."

## 4. Filesystem batching (validates and refines earlier design)

- **The debounce → coalesce → single-reindex pattern is the converged industry solution** — every serious implementation surveyed (custom build-loop watchers, Python's `watchfiles`, Rust's `notify`/`notify-debouncer-mini`, various MCP/agent-facing code-context tools) uses some version of this same shape. This validates the design direction rather than suggesting an alternative.

- **A single logical save is not a single filesystem event.** A raw watcher will report a short burst of multiple low-level operations for what a person considers "one save" — the debounce layer's entire job is to interpret bursts of raw events as one logical change, not to react to each raw event.

- **Concrete debounce numbers used in practice** cluster in the 100ms–2s range depending on the tool's purpose — diagnostics-oriented tools tend toward the low end (~150ms, to stay responsive), heavier reindexing-oriented tools toward the higher end (1–2s). Worth having this be configurable rather than hardcoded, since "how snappy should diagnostics feel" versus "how much do we want to avoid redundant reindexing work" is a real tuning tradeoff, not a fixed constant.

- **A dual-parameter debounce (a max wait for the first event, plus a separate "quiet period" window that resets on each new event within it) is the pattern used by mature file-watching libraries**, rather than a single fixed window — this bounds worst-case latency (never wait forever for true quiet) while still coalescing genuine bursts well.

- **rust-analyzer's own real-world experience directly validates the "lower priority for out-of-editor changes" design already settled on**: they explicitly built a custom watcher and deprioritized scheduling for out-of-editor/agentic changes specifically because naive immediate reindexing on every agent-driven change caused visible UI artifacts (inlay hints jumping/going stale). This is not a hypothetical concern — it's an observed problem in an existing, widely-used tool with the same shape of workload this project targets.

- **Recursive filesystem watching has real OS-level costs.** Watching a large directory tree recursively consumes per-entry kernel resources and can hit platform watch limits (this is why `node_modules`-sized directories are a known pain point). The engine should exclude ignorable directories (build output, dependency directories, VCS internals) from the watch scope proactively, ideally sourced from the same project-manifest awareness used for import resolution, rather than watching everything and filtering after the fact.

## 5. Miscellaneous points worth carrying into design

- **Real agent-facing LSP tooling already treats "is this file currently open/interactively tracked" as a meaningful branch point** in scheduling and change-handling logic (seen in existing MCP-to-LSP bridge tools) — this matches and reinforces the earlier decision to prioritize open/visible files over background-only ones.
- **Diagnostics-specific debounce is sometimes handled separately from general reindex debounce** in existing tools, since diagnostics are usually the most latency-sensitive, human-visible signal — worth considering whether diagnostics deserve their own (shorter) debounce window distinct from the general reindex debounce, rather than sharing one timer for everything.
- Tree-sitter's grammar-authoring language itself is JavaScript-based (grammars are written as JS files and compiled to C), which is worth knowing for the plugin-authoring documentation even though the runtime consuming the grammar is pure C — plugin authors bringing their own grammar will need a JS-based grammar toolchain step at plugin-build time, separate from the Lua configuration layer used at runtime.

---

**Where these land in the build:** position encoding → [`docs/engine/position-encoding.md`](../engine/position-encoding.md) · filesystem bursts → [`docs/engine/filesystem-changes.md`](../engine/filesystem-changes.md) · type solving → [`docs/spec.md`](../spec.md) §6.1 Tier 3 · graph traversal → [`docs/graph-lite/README.md`](../graph-lite/README.md).
