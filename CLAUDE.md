# Universal LSP + graph-lite

One C++ language server for many languages (Lua-declared plugins, no external
language-server processes), plus **graph-lite** — the embeddable graph library it
runs on. Built for a terminal-first, agent-driven world; standard LSP for editors,
MCP for agents.

**Status:** pre-implementation. Repo infrastructure, gates and docs are in place;
build phase 1 (graph-lite core) is the current work. Scope: [`docs/spec.md`](docs/spec.md).

**This repo is AI-first.** Agents write the code; humans review at the PR. Every
rule below exists to make that loop fast and safe.

## Response rules

- Execute. No preamble, no "I'll start by…", no restating the task.
- Lead with action or answer. Reasoning after, only if non-obvious.
- Batch independent tool calls. Read several files at once, grep in parallel.
- Never speculate about code you have not read. Read first.
- Disagree when the user is wrong. State the correction, once.
- Terse. Fragments fine. Commands, paths and code stay exact.
- End summary: 1–2 sentences.

## Coding rules

- **Fail fast, loudly.** A crash on a bad assumption beats a wrong answer. Invalid
  config throws; it never silently degrades. Validate at the seam where the bad
  value enters, not three layers down.
- **Optimize for the next debugger.** Errors name the value, the seam, and the fix.
  One seam per invariant — one place to look, one place to fix.
- **Simplicity.** Minimum code that solves the stated problem. No speculative
  abstraction, no config knob nobody asked for, no error handling for impossible
  cases. Concrete first; introduce an interface when the SECOND implementation
  actually arrives.
- **Surgical.** Every changed line traces to the request. No drive-by reformatting.
  Match surrounding style.
- **Verifiable done.** Convert the task to a command that proves it. Bug →
  reproducing test first. Refactor → green before AND after.
- **TDD is the working method here**, not an aspiration. Write the failing test,
  then the code. This project has no UI to eyeball and no staging to poke: a test
  is the only thing that says "done".
- **Ship the guard with the fix.** A defect that can recur gets its mechanical
  guard — a test, an assertion at the seam, a `scripts/lint/` rule — in the SAME
  PR. Prose telling the next agent to be careful is a bug report against tooling.
- **Never ship a fix you know is wrong.** Say so in one sentence and take the right
  path. Never silence a failing test, a warning, or a guard to get green — the
  guard is the asset. If the quick fix is chosen anyway, pin the known-bad
  behaviour with a test; never a bare `TODO`, nobody greps those.
- **One path, replaced completely.** No shims, no dual paths, no feature flags,
  no `v2` beside `v1`. Public contracts (the LSP surface, MCP tool names, the
  graph-lite header, the persistence format) are the genuine exception — those are
  one-way doors; name them and think before shipping.

## The project

Two products, one repo, one direction of dependency.

| | What | Depends on |
|---|---|---|
| **graph-lite** | Embeddable property-graph library. Typed nodes, typed directed edges, properties. CSR storage, native traversal, mmap-friendly persistence. **Public surface is a C ABI** (`graph-lite.h`); `graph-lite.hpp` is header-only C++ ergonomics over it. MIT. | nothing |
| **Universal LSP** | The language server: stdio LSP + MCP + batch CLI, tree-sitter parsing, native scope/type engines, Lua plugins. Apache-2.0. | graph-lite |

**The doctrine that outranks instinct:**

- **No external processes at runtime.** The engine never spawns, proxies to, or
  depends on rust-analyzer / pyright / clangd / any binary or network service. A
  language degrades to fewer features when its plugin rules are thin — never
  because something is not installed.
- **graph-lite knows nothing about this consumer.** No LSP concept in its API,
  internals, tests, or docs. Both directions are enforced by guards; see below.
- **graph-lite's contract is C, not C++.** Every embedder — Ruby, JavaScript,
  Python, Rust, Go — binds `graph-lite.h` through an FFI. No C++ type may cross
  that boundary, and `graph-lite.hpp` is a convenience layered on top, never the
  other way around ([ADR 0004](docs/architecture/decisions/0004-graph-lite-c-abi.md)).
- **Agents are the primary client, editors the compatible one.** Design request
  shapes for "what breaks if I change this" as much as for hover-on-cursor.

## Layout

| Path | What | Notes |
|---|---|---|
| `graph-lite/` | The library. Own LICENSE, own version, own presets. | Must build standalone: `cmake -S graph-lite -B build/x -G Ninja` |
| `engine/` | Daemon core: transport, sessions, workspaces, worker pools, scheduler, file-watch pipeline, Lua embedding, MCP front-end. | |
| `analysis/` | Translation layer: tree-sitter + plugin rules → graph-lite's generic API. | **The only place both vocabularies may meet.** |
| `plugins/` | One dir per official language, Lua rule files. | Community plugins live out of repo, by design. |
| `daemon/` `cli/` | Thin entrypoints. Wiring, no logic. | |
| `tests/` | `graph-lite/` · `engine/` · `plugins/` — mirrors the boundaries. | |
| `docs/` | Same boundary. `docs/graph-lite/` reads as if standalone. | |
| `scripts/` | Bun-TS automation: the gate, the change detector, the guards. | `bun run scripts/help.ts` |
| `bin/` | Thin shims over `scripts/`. | |

## Stack

| Layer | Choice | The non-obvious constraint |
|---|---|---|
| Language | C++20 | No Boost, except possibly `Boost.Asio` for the daemon event loop. Everything else is the standard library. |
| Build | CMake ≥ 3.24 + Ninja, presets only | `dev` · `asan` · `release`. Never hand-roll a `cmake` invocation that a preset covers. |
| Tests | GoogleTest + CTest | System GTest if present, pinned fetch otherwise. |
| Memory | arena / `std::pmr` on hot paths | Which is exactly why ASan + UBSan run on every PR, not after the first mystery crash. |
| Parsing | tree-sitter (C) | Grammars are authored in JS and compiled to C — a plugin-author build step, not a runtime dependency. |
| Plugins | Lua | Declarative rule tables + small pure callbacks. Callbacks get an instruction/time budget; a runaway one must not hang the daemon. |
| Tooling | Bun + TypeScript under `scripts/` | Repo automation only. Ships in no binary. |

## The loop

Three tiers. Run the tightest one that answers your question.

| Tier | Command | Scope | Budget |
|---|---|---|---|
| Tightest | `bin/check --files <paths>` | format + guards for those files | < 3s |
| Scoped | `bin/check --changed` | the affected subsystems, dependents included | < 30s |
| Full gate | `bin/check` | everything, sanitizers included — the CI mirror | ~1 min |

**Do NOT reflexively run `bin/check`.** It configures three build trees and runs
the suite twice, once under ASan + UBSan. Run it once, before pushing.

**Fail one, fix one.** Stop at the first failure, fix it, re-run only that command.
Never trust a piped exit code — `cmd | tail; echo $?` reports `tail`'s status.

**What the scoped tiers do NOT see:** a break in an unaffected subsystem, and
anything only the sanitizer build catches. That is an acceptable trade *because*
the full gate runs before push.

## Commands

```bash
bin/setup                       # prerequisites → configure build/dev → link compile_commands.json
bin/dev build [preset]          # build (dev | asan | release)
bin/dev test [regex]            # build + run the suite, or just matching tests
bin/dev asan                    # configure + build + test under ASan/UBSan
bin/dev run <target>            # build + run one binary
bin/dev doctor                  # one row per thing that can drift
bin/check [--files … | --changed | --fast]
bin/fmt                         # clang-format --write over tracked C/C++
bun run scripts/lint/index.ts   # repo guards only
bun test scripts/               # the guards' own tests + the change detector's
bun run scripts/help.ts         # the full catalog
```

## Code patterns

- **RAII for every lifecycle-heavy resource** — sessions, workspace state, worker
  pools, Lua interpreter states, graph handles. Construction acquires, destruction
  releases; that is also what makes the test fixtures cheap.
- **Custom error types, never a bare `throw std::runtime_error`.** A caught type
  is self-documenting; a string is not.
- **Files ≤ 500 lines.** One file, one job. Guard: `scripts/lint/max-file-loc.ts`.
- **Conversion at the protocol boundary only.** Internally the engine speaks one
  encoding; UTF-16 offsets exist at the wire and nowhere else.
- **No new warning suppressions.** `-Werror` is on. Fix the code, not the flag.

## Hard boundaries

- Never add a runtime dependency on another language server, binary, or network
  service. This is the product's whole thesis.
- Never make graph-lite depend on anything in this repo — no include, no link, no
  concept name.
- Never put a C++ type, an exception, or a published struct layout in
  `graph-lite.h`. It is an ABI every binding pins; changing an entry point breaks
  them silently, at load time.
- Never disable a guard, a warning, or a test to get green.
- Never introduce a feature flag or a second code path "for safety".
- Never commit a secret, a personal path, or a machine-specific value.

## Preventive rules (these bite before any symptom appears)

- **A `Position` offset is UTF-16 code units on the wire and something else
  internally.** Get the conversion wrong and the failure is a *panic on an emoji*
  in someone else's editor, months later — this exact bug shipped in a production
  Rust language server. Convert at the boundary, negotiate `positionEncoding` at
  `initialize`, and test astral-plane characters from the first commit.
- **An incremental tree-sitter reparse is not guaranteed to equal a fresh parse of
  the same text.** Documented, reproduced behaviour: spurious error nodes survive
  an edit. Any incremental path needs a periodic full reparse as a safety net,
  or diagnostics drift from reality with nothing to indicate it.
- **The occurs check is mandatory in unification, not an optimisation.** Unifying
  `T` with a type containing `T` without it recurses forever — it hangs the worker
  thread, and the symptom is "the editor stopped responding", not an error.
- **A single save is not a single filesystem event.** Reacting per raw event
  reindexes 3–5× per save. Debounce with two parameters (max wait + resetting
  quiet period), then coalesce, then diff by content hash — mtime lies about
  renames.
- **Out-of-editor changes are the DEFAULT case here, not the exception.** An agent
  editing files on disk sends no `didChange` at all. Anything that assumes
  protocol-level edit signals will simply not run for the primary user.
- **Background work for a file must be cancellable and superseded** the moment
  that file becomes interactively relevant. rust-analyzer had to retrofit exactly
  this after agent-driven reindexing made inlay hints visibly jump.
- **`bun test scripts/` is not optional before pushing a workflow change.**
  `scripts/ci-parity.test.ts` is what keeps "green locally" meaning anything.

## Docs index — read before touching that area

| Read | Before |
|---|---|
| [`docs/spec.md`](docs/spec.md) | anything. It is the scope contract. |
| [`docs/research/things-to-know.md`](docs/research/things-to-know.md) | designing a subsystem — it is the list of mistakes already paid for |
| [`docs/graph-lite/README.md`](docs/graph-lite/README.md) · [`boundary`](docs/architecture/graph-lite-boundary.md) | touching `graph-lite/` |
| [`docs/engine/position-encoding.md`](docs/engine/position-encoding.md) | anything touching offsets, ranges or text |
| [`docs/engine/filesystem-changes.md`](docs/engine/filesystem-changes.md) | the watcher, debounce, or reindex scheduling |
| [`docs/plugins/authoring.md`](docs/plugins/authoring.md) | writing or changing a language plugin |
| [`docs/standards/testing.md`](docs/standards/testing.md) | adding a test suite |
| [`docs/standards/code-style.md`](docs/standards/code-style.md) | the C++ conventions the linters do not encode |
| [`docs/gotchas.md`](docs/gotchas.md) | you have a symptom and want the cause |
| [`docs/architecture/decisions/`](docs/architecture/decisions/) | reopening a settled design question |

Conventions this repo inherits: `../gold-standards-in-ai/docs/` (not vendored —
a sibling checkout).

## PR conventions

- Branch: `<type>/<slug>` — `feat/csr-storage`, `fix/utf16-surrogate-offset`.
- Commits: conventional (`feat:`, `fix:`, `docs:`, `chore:`, `test:`, `refactor:`).
- One concern per PR. Cap ~110 changed files — automated review skips past a
  limit, so the riskiest PR would get the least review.
- `Closes #N` in the body. Plan slice → issue → PR is one continuous line.
- Never push red. `bin/check` before push, in the background if it is slow.
- A PR with 0 checks registered is not a passing PR — wait for them to attach.

## What NOT to build

| Rejected | Why |
|---|---|
| Proxying to an existing language server | Deleting that dependency IS the product. A "temporary" proxy would become the architecture. |
| A query language for graph-lite (Cypher/SQL/Datalog) | Not until real usage says which queries matter. A programmatic traversal API first. |
| SQL recursive CTEs for traversal | Benchmarked 100×+ slower — relational engines cannot keep visited-set state across iterations. Traversal stays native. |
| TCP/socket LSP transport as a first-class feature | stdio is what every editor and agent harness actually spawns. |
| Cross-session priority/fairness scheduling | FIFO per pool, deliberately. One aggressive agent session CAN delay another's request; that is a known, accepted v1 trade. Cheap refinement later: two queues (focused document vs everything else). |
| Distributed / clustered / server-mode graph-lite | Embedded, in-process, forever. |
| Feature flags | N flags = 2^N states nobody has run. Use a config record with a schema and an owner, or ship one path. |
