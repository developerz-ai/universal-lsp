# Universal LSP & graph-lite — Project Specification

> The source of truth for scope. Written before implementation; amended by ADRs
> under [`docs/architecture/decisions/`](architecture/decisions/) rather than
> edited into disagreement with the code. Research that informed it:
> [`docs/research/things-to-know.md`](research/things-to-know.md).

## 1. Vision

This is a monorepo containing two related but independently useful projects:

- **The Universal LSP engine**: a single, fast, low-memory language server, written in C++, providing Language Server Protocol support for many programming languages through a Lua-based plugin system. Every analysis tier, including semantic/type analysis, is implemented natively — there is no dependency on external language server processes, subprocesses, or network services at runtime. The engine is built primarily for a terminal-first, AI-agent-driven future where code is frequently read and edited without a human-operated text editor in the loop, while remaining fully compatible with traditional editors via standard LSP.
- **graph-lite**: a small, fast, embeddable C++ graph storage and traversal library — conceived as the missing "SQLite for graphs": a lightweight, dependency-free, in-process graph engine that any C++ project can embed via a single public header, filling a real and currently unmet gap in the open-source ecosystem (the most prominent prior embeddable graph database, Kùzu, was abandoned by its maintainers in October 2025, leaving no actively-maintained, vendor-independent, natively-embeddable open-source option).

The Universal LSP is graph-lite's first, heaviest, and most demanding real-world consumer: it builds and queries call graphs, import graphs, and symbol relationship graphs at scale, across many languages and large codebases, under real latency constraints. graph-lite is designed and built to serve that workload well, but its public API is kept deliberately generic and free of any LSP- or code-specific concepts, so that it is useful as a general-purpose embeddable graph library on its own, and so that it can eventually be extracted into its own standalone repository/release with minimal friction, once it has been proven under real usage inside the LSP.

## 2. Relationship between the two projects

- **graph-lite has no dependency on the LSP engine.** Dependencies flow one direction only: the LSP engine depends on graph-lite, never the reverse. graph-lite must never reference LSP-specific concepts (symbols, scopes, documents) in its public API or internals — those concepts are translated into graph-lite's generic node/edge model by a separate translation layer that lives in the LSP engine's own codebase.
- **graph-lite is deliberately not launched or marketed as a stable, general-purpose database product on day one.** It is developed in the open, in this repository, under its own license and versioning scheme from the start, but its "prove it under real, demanding usage first" phase happens as part of building the LSP. A future decision to extract it into its own repository and formally release it as a standalone product is made after that real-world usage has validated (and likely reshaped) its API and edge-case handling — not before.
- **The LSP is free to use more than one storage/query technology for different purposes**: graph-lite is the primary, latency-critical, in-process structure for call graphs, import graphs, and relationship queries (blast radius, call paths, workspace-wide symbol graphs). A separate, optional, non-critical-path store (SQLite, or an embeddable analytical engine with graph query support) may be used purely as an ad hoc, exportable query/debugging surface for humans and tooling — never as the source of truth, and never on any latency-sensitive request path.

> **Enforced, not merely stated.** Two guards run on every PR:
> `scripts/lint/graph-lite-boundary.ts` (no include or link crosses back) and
> `scripts/lint/graph-lite-vocabulary.ts` (no borrowed concept names in the
> library, its tests, or its docs). The `standalone` CI job builds graph-lite
> with no knowledge of this repo. See [`docs/architecture/graph-lite-boundary.md`](architecture/graph-lite-boundary.md).

## 3. Goals

- One LSP binary/daemon, one protocol implementation, reused across every supported language.
- A small, dependency-free, embeddable graph library (graph-lite) usable both internally by the LSP and independently by other C++ projects via a single public header.
- Low memory footprint and low latency for the LSP, competitive with or better than existing per-language servers.
- A plugin architecture that lets a language be fully defined declaratively in Lua, without writing C++, and without depending on any other running process.
- Official first-class LSP support for: TypeScript, JavaScript, Ruby, C, C++, Lua, Python, Java, Kotlin, Swift, Rust.
- A community plugin ecosystem distributed via git repositories, discoverable and installable without a central build step.
- First-class support for a terminal-only, editor-less, AI-agent-driven usage pattern, alongside continued compatibility with traditional editors: batch/query-style access, multi-file burst edits, and a way for an agent to know when analysis has caught up with its changes.
- Graceful, transparent degradation: a language plugin can offer only basic features or full semantic features purely as a function of how complete its Lua-declared rules are — never as a function of whether some other program is installed or reachable.

## 4. Non-goals

- Depending on, spawning, or proxying to external language server processes (rust-analyzer, pyright, clangd, etc.) or any other subprocess/network service as part of normal LSP operation.
- Building graph-lite into a distributed, clustered, or server-mode database. It is an embedded, in-process library only.
- Building a new text editor or IDE.
- Supporting graphical/projectional languages — text-based languages only.
- Launching graph-lite as a marketed, versioned-1.0, externally-relied-upon product before it has been proven inside the LSP under real workloads.

## 5. graph-lite — design

### 5.1 Scope and API shape

- Public interface exposed via a single header (`graph-lite.h`, mirroring the ergonomics of single-header/amalgamation-style C/C++ libraries like SQLite's), so any project can embed graph-lite with minimal integration friction.
- **That header is C, with C linkage, and it is the contract** — opaque handles, integer status codes, caller-owned buffers, no C++ type crossing the boundary. The SQLite comparison is not only about being in-process: SQLite's reach comes from a flat C ABI that every language runtime can call. Ruby (FFI), JavaScript (N-API / `bun:ffi`), Python (`ctypes`/`cffi`), Rust, Go and C all bind the same entry points. A C++-only surface would cap adoption at C++ projects and would not even link reliably across toolchains, since C++ has no stable ABI. A separate header-only `graph-lite.hpp` provides C++ ergonomics over the same entry points and exports nothing of its own. Rationale and the rules that follow: [`docs/architecture/decisions/0004-graph-lite-c-abi.md`](architecture/decisions/0004-graph-lite-c-abi.md).
- Generic property-graph data model: typed nodes and typed, directed edges, both carrying arbitrary key/value properties. No built-in concept of "symbol," "file," or "function" — those are just node types a consumer defines.
- Core operations: add/remove nodes and edges, traverse (BFS/DFS, bounded and unbounded depth), reverse-edge traversal (for blast-radius/impact-style queries), shortest/any path between two nodes, and basic filtering by node/edge type during traversal.
- No query language in the initial version (no Cypher/Datalog/SQL layer) — the library exposes a programmatic C++ traversal API. A declarative query layer can be considered later, once real usage clarifies what's actually needed, rather than designed speculatively up front.

### 5.2 Storage and performance

- Primary in-memory representation: a CSR-style (compressed sparse row) adjacency structure — flat, contiguous arrays for node and edge data, chosen specifically because it is cache-friendly for traversal and naturally maps to a fast, near-zero-parse on-disk persisted format.
- Persistence format designed to be structurally close to the in-memory layout, so a reload can be close to a direct read/mmap rather than a full deserialize-and-rebuild pass — consistent with the LSP's own fast-restart goals.
- Explicitly avoid the SQL-recursive-CTE approach to traversal: research and real-world benchmarking consistently show that relational recursive-query engines cannot maintain visited-node state efficiently across iterations, leading to dramatic (100x+) slowdowns on deep or highly-connected graph traversal compared to a native in-memory adjacency-list traversal. graph-lite's core traversal engine must be native, not SQL-backed.
- Single-writer-many-reader concurrency for an initial version is an acceptable starting scope, but the multi-agent usage pattern this project targets makes concurrent multi-writer support a meaningfully valuable differentiator versus prior art in this space (the most prominent predecessor library had a hard single-writer limitation, which was explicitly called out as a gap by at least one downstream fork targeting multi-agent use cases). Worth designing the concurrency model with eventual multi-writer support in mind, even if v1 ships single-writer.

### 5.3 Testing

- graph-lite gets its own independent test suite, expressed purely in terms of its generic node/edge model — no LSP concepts should appear anywhere in graph-lite's tests, as a forcing function for API cleanliness.
- The C ABI claim is tested, not asserted: at least one translation unit in the suite is compiled **as C** and linked, so a header that stops being C-compatible fails the build rather than failing an embedder. Reference bindings in a scripting language (build phase 13) are the honest test of whether a C surface designed by C++ programmers is actually usable from outside.
- Correctness testing should include fuzzing and crash-consistency testing of the persistence layer specifically — this is the highest-risk area for any embedded storage engine (silent data corruption is a severe failure mode) and deserves disproportionate testing investment relative to the traversal algorithms themselves.

## 6. Universal LSP — high-level architecture

### 6.1 Capability tiers

The system is organized in tiers per language, each defined entirely through a plugin's Lua configuration and executed natively by the C++ engine:

- **Tier 1 — Syntax**: parsing, syntax highlighting, code folding, document outline, bracket matching. Backed by tree-sitter (itself a C library, no runtime dependency conflict). Near-zero-effort for any language with an existing tree-sitter grammar.
- **Tier 2 — Structure**: scope resolution, symbol binding, go-to-definition, find-references, rename, workspace symbol search. Driven by a declarative scope/binding rule set interpreted by the engine's native scope-resolution algorithm, with optional pure Lua callback functions for irregular cases.
- **Tier 3 — Semantics**: type-aware hover, type-checking diagnostics, semantic completions. The engine provides a shared native constraint-generation and unification/solving core (union-find-backed, in the tradition of Hindley-Milner-style inference engines), which plugins configure via declarative type rules plus optional Lua callbacks for irregular constructs. This is the highest-risk, most novel subsystem in the project and should be validated early on a simple language before broad rollout.
- **Tier 4 — Relationship graph**: call graphs, import/dependency graphs, and other cross-file/cross-symbol relationship data, materialized into graph-lite as the LSP's analysis proceeds. This tier is what enables agent-facing queries like "what calls this function, transitively," "what breaks if I change this," and "what's the path between these two symbols" — capabilities directly informed by the current wave of "codebase knowledge graph for AI agents" tools, several of which have seen rapid, large-scale open-source adoption specifically because agents benefit enormously from pre-computed, queryable structure instead of re-deriving it via repeated file reads and text search.

### 6.2 Interfaces exposed to clients

- **Standard LSP over stdio** — the universal transport standard for language servers; TCP/socket support is not needed as a first-class feature. This serves both traditional editors and any agent harness that specifically wants raw LSP compliance (some already exist, e.g. terminal-based coding agents with native LSP tool support).
- **An MCP (Model Context Protocol) server interface, exposed as a co-equal front-end alongside LSP**, not an afterthought. In an editor-less, agent-only usage pattern, MCP is how the current generation of AI coding agents already consume structured code-intelligence tools. The MCP interface should expose batch/query-shaped operations well-suited to how agents actually ask questions (impact radius of a change, call path between two symbols, architecture/dependency overview) directly backed by the Tier 4 relationship graph, rather than forcing an agent to compose many individual LSP requests to answer a single composite question.
- **An optional headless batch-query CLI mode**, separate from both the daemon/LSP and MCP paths — point it at a repository, ask for a single structured answer (diagnostics, symbol lookup, impact radius) as one command returning JSON, no session/handshake required. Useful for CI and for the simplest possible agent integrations.

### 6.3 Process architecture: daemon plus thin client shim

- The engine runs as a single, persistent background daemon per machine/user, not a fresh process per editor window or per invocation. This preserves warm caches and avoids repeated cold-start cost across multiple editor windows, multiple simultaneously-open unrelated projects, and multiple concurrently active AI agent sessions.
- Editors and agent harnesses that expect to spawn a stdio-speaking child process are given a small stdio shim binary: on startup it attempts to connect to the daemon's well-known local socket (Unix domain socket / named pipe, never a network-exposed port); if absent, it spawns the daemon (using a lock/pidfile to avoid a startup race spawning duplicates), waits for the socket, then connects and transparently forwards bytes both directions. From the editor's perspective, this is indistinguishable from any other stdio-based language server.
- The daemon multiplexes multiple concurrent client sessions (editor windows, agent processes, MCP clients) over one process. Each session gets its own logical LSP/MCP handshake and open-document set, but sessions attached to the same project root share the same underlying workspace analysis state (symbol tables, type caches, relationship graph), reference-counted so state can stay warm for a grace period after the last session on a project disconnects, in case it reconnects soon after.
- Workspace states for different, unrelated projects are fully independent within the daemon — no cross-project locking, and an internal memory budget with least-recently-used eviction protects against one large project starving others sharing the daemon.
- The shim detects a dead or version-mismatched daemon (via a version handshake) and transparently respawns/reconnects, so editors see this as an ordinary server restart.
- Request scheduling across concurrent sessions uses simple first-in-first-out ordering within each worker pool — no cross-session priority/fairness logic in the initial version. This is an accepted, documented simplification: it means a very large burst of requests from one aggressive agent session can measurably delay another session's (including a human's) interactive request, which is a known and intentional tradeoff for implementation simplicity, not an oversight. A cheap future refinement, if needed, is a two-queue split (currently-focused-document requests versus everything else), without building full multi-level fairness.

### 6.4 Concurrency model

- An I/O thread owns transport decode/encode only (no parsing or analysis on this thread).
- A small interactive worker pool handles analysis for currently open/focused documents, keeping hover/completion/diagnostics latency low; each open document is consistently routed to the same worker thread to avoid cross-thread locking of its syntax tree.
- A separate, lower-priority background worker pool handles whole-workspace indexing and reindexing.
- Because the engine has no external subprocesses to wait on, worker pool sizing should be tuned around CPU-bound cost (parsing, scope resolution, type solving), not I/O wait.
- Shared analysis state (symbol index, scope tables, resolved type tables, the graph-lite-backed relationship graph) should favor reader-friendly concurrent structures (sharded maps, snapshotting) over broad locks, so background reindexing does not stall interactive queries.
- In-flight background jobs for a file must be cancellable and superseded immediately if that file becomes interactively relevant mid-job.

### 6.5 Handling filesystem changes: a single, unified detection path

Earlier framing of "keystroke edits vs. bulk external changes vs. agent batch edits" as three cleanly distinguishable categories understates a key reality: **when an AI agent edits files directly on disk — which is the common, expected case for a terminal-only, editor-less future — there is no `didChange` protocol signal at all**, only raw filesystem events indistinguishable from a `git pull` or any other external tool. The only case with genuinely distinct, rich signal is a live editor session sending real `didChange` notifications per keystroke.

The design should therefore have two real paths, not three:

1. **Interactive `didChange`-driven editing** (only occurs when a live editor client is actually attached to a session): small, frequent, single-file edits. Use incremental reparsing of just the changed region and shallow, fast reanalysis, deferring full re-binding/re-typing/re-indexing until save or a short idle period. A periodic full fresh reparse (not just incremental) should run on a cadence or after N incremental edits, as a correctness safety net — incremental parsing is not guaranteed to converge to the same tree as parsing the final text from scratch, and this is a documented, real failure mode in tree-sitter and comparable incremental parsers, not a hypothetical edge case.

2. **Filesystem-burst detection** (covers git operations, external tools, and — critically — the default/expected AI agent case of direct disk writes): a file-watcher subsystem (inotify/FSEvents/kqueue, non-recursive where feasible, with ignorable directories excluded from the watch scope, ideally sourced from project-manifest awareness) feeds a debounce/coalescing pipeline with two parameters — a maximum wait for the first event, and a separate "quiet period" window that resets on each new event within it, bounding worst-case latency while still coalescing genuine bursts. Content-hash diffing (not just modification time) determines what actually changed, distinguishing renames/moves from delete+create pairs. The resulting batch is reindexed as one coalesced job, prioritizing currently open/visible files for immediate reanalysis and queuing everything else at background priority. Background jobs triggered this way run at lower scheduling priority than any file in current interactive focus — directly informed by real-world experience from an existing Rust language server that had to add exactly this deprioritization after observing that naive immediate reindexing on every agent-driven change caused visible artifacts.

- The engine should expose an explicit "settle" / "wait for idle" request as a custom LSP extension (and equivalently as an MCP tool), which a client can call after a batch of edits, resolving once debouncing and reanalysis for that batch have completed — giving an agent a deterministic signal that diagnostics/symbols reflect its latest edits, instead of polling or guessing with delays.

## 7. Plugin system

- A plugin is a self-contained unit, written entirely in Lua, declaring support for one or more languages by file extension/type. A plugin never itself spawns a process, opens a network connection, or depends on any external binary — everything is expressed as declarative rule tables and small pure rule-callback functions evaluated by the engine's native machinery.
- A plugin declares, for each tier it supports:
  - **Tier 1**: tree-sitter grammar reference, plus query files/rule sets for highlighting, folding, and symbol extraction.
  - **Tier 2**: declarative scope/binding rules (which node types open scopes, bind names, or are references), plus optional pure Lua callbacks for irregular cases (imports, macros, dynamic scoping quirks).
  - **Tier 3**: a declarative type rule set describing how literals, expressions, and calls produce and constrain types, in terms the engine's shared native type-solving core understands, plus optional Lua callbacks for constructs too irregular for the declarative rules. Coverage can and should start as a reduced, best-effort subset and expand over time; unsupported constructs should degrade to "unknown type" rather than being wrong.
  - Static completion sources (keywords, common snippets) and optional formatting rules.
- Plugin resource limits: a runaway Lua callback (infinite loop, unbounded recursion) must not be able to hang or unboundedly grow memory in the daemon — a simple instruction-count or time budget per callback invocation is sufficient; a full sandboxed VM is not necessary.
- Plugins declare a minimum required engine version, so the engine can detect and warn about incompatibility.
- Multiple plugins can be loaded simultaneously within one workspace, each scoped to its own file types.

## 8. Distribution and plugin management

- Plugins are distributed as git repositories, installable by URL, pinned to a commit/tag for reproducibility, with official language plugins living in this monorepo's `plugins/` directory and community plugins distributed entirely independently, with no approval step required.
- Projects declare their plugin dependencies and pinned versions in a checked-in lockfile-like manifest.
- Because no plugin depends on external tools, a project's tooling is fully reproducible from pinned plugin commits alone.
- A simple, optional curated index (a hosted list mapping short names to git URLs) can be added later purely for discoverability.

## 9. Repository structure

A single monorepo, structured so graph-lite could be extracted with minimal friction later:

- **`graph-lite/`** — the embeddable graph library. No dependency on anything under `engine/`. Own LICENSE and versioning scheme, distinct from the LSP's, even while living in this repo. Own build target and own test suite, expressed purely in generic node/edge terms with zero LSP-specific vocabulary anywhere in its code, tests, or documentation.
- **`engine/`** — the LSP daemon core: transport, session/workspace management, worker pools, scheduler, file-watcher/debounce pipeline, Lua embedding, MCP server interface. Depends on `graph-lite/`.
- **`analysis/`** — the translation layer that turns tree-sitter output and plugin Tier 2/3 rules into calls against graph-lite's generic API (turning "symbol" into "node," "calls" into "typed edge," etc.). This is where LSP-specific concepts live; graph-lite itself stays unaware of them.
- **`plugins/`** — one subdirectory per officially supported language, each containing that language's Lua rule files. Community plugins are out-of-repo by design.
- **`daemon/`** / **`cli/`** — thin binary entrypoints (the daemon process, the stdio shim client, the batch-query CLI mode) that wire the above together; minimal logic of their own.
- **`tests/`** — mirrors the repo's internal boundaries: `tests/graph-lite/` (pure graph-engine correctness and fuzz/crash-consistency tests), `tests/plugins/` (per-language golden-file/snapshot tests — feed a plugin a sample file, assert the resulting scope tree/highlights/diagnostics match a checked-in expected output), `tests/engine/` (protocol-level and scheduling tests).
- **`docs/`** — mirrors the same boundary; `docs/graph-lite/` is written as though graph-lite is already a standalone library (API reference, data model, persistence format), both as useful documentation now and as a forcing function that catches any accidental LSP-concept leakage into graph-lite's design.

## 10. Development approach: TDD, C++

- The project is built in modern C++ (17/20), without a broad Boost dependency. `Boost.Asio` may be pulled in narrowly if needed for the daemon's local-socket/timer event loop, but everything else (threads, containers, variant/optional, arena/pmr allocation for hot-path memory locality) is well covered by the standard library.
- Test-driven development is the primary working method. Use GoogleTest or Catch2 with CMake/CTest integration, and wire AddressSanitizer/UndefinedBehaviorSanitizer into the test build from the start — critical given the manual arena/pmr memory management this project relies on for performance.
- RAII should be used consistently for all lifecycle-heavy resources (daemon sessions, workspace states, worker pools, Lua interpreter states, graph-lite handles), both for correctness and because it directly supports fast, isolated, repeatable unit tests via constructor/destructor-based fixtures.

## 11. Protocol-level correctness requirements

- **Position encoding**: LSP historically mandates UTF-16 code-unit offsets for `Position`, a legacy of VS Code's JavaScript implementation, even though document content is transmitted as UTF-8 and this engine's internal representation will not be UTF-16. This mismatch is a well-documented, real source of shipped bugs across the LSP ecosystem (including at least one confirmed case of a production Rust language server crashing on certain multi-byte emoji due to an offset landing mid-character). The engine must implement correct UTF-8/UTF-16/codepoint offset conversion, negotiate the `positionEncoding` client capability at `initialize` time (LSP 3.17+) and prefer UTF-8 or UTF-32 internally where the client supports it, and this conversion logic needs explicit, dedicated test coverage against multi-byte and surrogate-pair characters from the start — not treated as an edge case to handle later.
- **Line-ending handling**: the spec recognizes `\n`, `\r\n`, and `\r` as valid end-of-line sequences, and a `Position` cannot legally reference a location inside a line-ending sequence; line-splitting logic must be consistent about this.
- **Multi-root workspace support** (`workspaceFolders` in `initialize`) should be implemented from the start, not bolted on later, since it's both a native part of the spec and directly useful for the daemon's multi-project design.

## 12. Officially supported languages at launch

TypeScript, JavaScript, Ruby, C, C++, Lua, Python, Java, Kotlin, Swift, Rust.

For each, Tier 3 type rule coverage should be built incrementally, starting with the common, everyday subset of the language's type system and expanding over time; languages with especially intricate type systems (C++ templates, Rust's trait solver, Swift's protocol conformance) should be expected to reach full coverage more slowly and should degrade gracefully to "unknown type" for unsupported constructs rather than being wrong. Prioritize solid Tier 1/2 coverage across all eleven languages first — this is tractable quickly and already delivers most of the value agents and editors need (navigation, structure, relationship graph population) — then invest incrementally in Tier 3 depth per language based on real usage. Languages with simpler, more regular lexical scoping (Lua, C, JavaScript/TypeScript) are good early candidates for proving the Tier 1/2 declarative rule system before tackling languages with more irregular binding semantics.

## 13. Non-functional requirements

- **Memory**: low steady-state memory usage relative to typical per-language servers, especially with multiple workspaces/languages active at once — avoid duplicated parsed/analyzed state, offload infrequently-needed cached data to disk.
- **Latency**: interactive requests for the currently focused document must not be blocked by background indexing or by other sessions' request bursts (within the accepted FIFO-scheduling limitation noted in 6.3).
- **Self-containment**: must run correctly with no network access and no other installed developer tools present, beyond the engine binary and plugin git repositories. Treat this as testable — run the suite in a sandboxed environment with no other language toolchains installed.
- **Isolation**: an issue in one workspace must not degrade responsiveness for a different, simultaneously open workspace.
- **Data integrity (graph-lite specifically)**: silent corruption is the most severe possible failure mode for an embedded storage engine; the persistence layer needs disproportionate fuzzing/crash-consistency test investment relative to other subsystems.
- **Observability**: expose engine state for debugging — memory usage, active plugins, in-flight background jobs, Tier 3 rule coverage per language, active daemon sessions.

## 14. Suggested build phases

Each phase is a GitHub milestone. `gh issue list --milestone "<phase>"` is the current state of one.

1. **graph-lite core**: build the generic node/edge model, CSR-style storage, native BFS/DFS/path/reverse-traversal, and the mmap-friendly persistence format, with its own independent test suite (including early fuzz/crash-consistency testing of persistence) — before any LSP-specific code depends on it.
2. **LSP transport and single-language proof of concept**: implement stdio transport, daemon process skeleton (without the shim/multiplexing yet), and a single hardcoded/minimally-pluggable language (e.g. Lua) supporting Tier 1 only.
3. **Plugin loading and declarative Tier 1**: generalize into the Lua plugin system; implement highlighting, folding, and outline generically from a plugin's tree-sitter grammar and query files.
4. **Tier 2 scope/binding engine**: generic scope-resolution algorithm driven by declarative plugin rules; goto-definition, find-references, rename, workspace symbols.
5. **Tier 4 relationship graph, first pass**: wire Tier 2 output into graph-lite via the `analysis/` translation layer — call graphs and import graphs for at least one language, enough to validate the analysis-to-graph-lite integration end to end.
6. **Tier 3 type engine, first pass**: design and build the shared native constraint-generation/unification core; validate end-to-end on one or two simple languages before expanding — this is the highest-risk, most novel subsystem and deserves early, focused validation.
7. **Daemon multiplexing and the stdio shim**: persistent daemon process, socket discovery/spawn-race handling, session multiplexing, per-workspace state sharing and eviction, version-mismatch handling.
8. **Concurrency and filesystem-change handling**: multithreaded worker pools, the unified filesystem-burst debounce/diff/coalesce/priority pipeline, the "wait for idle" extension (as both an LSP extension and an MCP tool).
9. **MCP interface**: expose Tier 4 relationship-graph-backed batch/query tools (impact radius, call path, architecture overview) as MCP tools, alongside the existing LSP surface.
10. **Git-based plugin distribution**: plugin installation from git URLs, version pinning, project-level lockfile.
11. **Official plugin set**: build out and tune all eleven launch-language plugins, prioritizing Tier 1/2 breadth first, then Tier 3 depth incrementally.
12. **Hardening and observability**: persistent on-disk caching, debugging/introspection tooling, graph-lite persistence fuzzing hardened further based on real usage patterns from the LSP itself.
13. **Community enablement**: plugin-authoring documentation (the declarative scope-binding and type-rule DSLs), example community-style plugins outside the official eleven to validate the plugin API for third parties.
14. **graph-lite extraction decision point**: once graph-lite has been proven under real, sustained LSP usage, evaluate whether and how to extract it into its own standalone, independently versioned and released repository — informed by real API friction points discovered during phases 1–12, not decided speculatively up front.
