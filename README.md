<h1 align="center">Universal LSP</h1>

<p align="center"><strong>One language server for every language — and the graph library underneath it.</strong></p>

<p align="center">
  <a href="docs/spec.md">spec</a> ·
  <a href="docs/README.md">docs</a> ·
  <a href="docs/graph-lite/README.md">graph-lite</a> ·
  <a href="docs/plugins/authoring.md">write a plugin</a> ·
  <a href="CONTRIBUTING.md">contributing</a>
</p>

<p align="center">
  <a href="https://github.com/developerz-ai/universal-lsp/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/developerz-ai/universal-lsp/actions/workflows/ci.yml/badge.svg"></a>
  <img alt="C++20" src="https://img.shields.io/badge/C%2B%2B-20-blue">
  <a href="LICENSE"><img alt="License" src="https://img.shields.io/badge/engine-Apache--2.0-green"></a>
  <a href="graph-lite/LICENSE"><img alt="graph-lite license" src="https://img.shields.io/badge/graph--lite-MIT-green"></a>
</p>

---

Two projects in one repo, built together because one needs the other:

**Universal LSP** — a single, fast, low-memory C++ language server covering many
languages through **Lua-declared plugins**. Every analysis tier, semantic analysis
included, runs natively in one process. It never spawns, proxies to, or depends on
rust-analyzer, pyright, clangd, or any other binary or network service. Built for a
terminal-first, agent-driven world: standard **LSP over stdio** for editors, an
**MCP** front-end for agents, and a headless **batch-query CLI** for CI.

**graph-lite** — the missing *SQLite for graphs*: a small, dependency-free,
embeddable property-graph library any C++ project can drop in via a single header.
The LSP is its first and most demanding consumer — call graphs, import graphs and
symbol relationships across large codebases, under interactive latency — but
graph-lite's API is kept free of any of that vocabulary, and is
[mechanically prevented from learning it](docs/architecture/graph-lite-boundary.md).

> **Status: pre-implementation.** The repository, gates, docs and plan are in
> place; build phase 1 (graph-lite core) is the current work. Nothing here ships
> yet. Scope lives in [`docs/spec.md`](docs/spec.md); progress lives in the
> [milestones](https://github.com/developerz-ai/universal-lsp/milestones).

## Why

Today every language brings its own server, its own process, its own memory
footprint and its own idea of what "go to definition" costs. Open four projects in
four languages and you are running a dozen processes that share nothing — no cache,
no index, no relationship graph. And none of them were designed for the client that
now does most of the reading and editing: an AI agent working in a terminal, with no
editor attached, making bursts of changes directly on disk.

Universal LSP is one process for all of it. Adding a language means writing
declarative Lua rules, not a new server.

## The tiers

A language gets exactly the depth its plugin's rules describe — never more, never
less, and never dependent on what is installed on the box.

| Tier | Gives you | Driven by |
|---|---|---|
| **1 — Syntax** | highlighting, folding, outline, bracket matching | a tree-sitter grammar reference + query files |
| **2 — Structure** | go-to-definition, find-references, rename, workspace symbols | declarative scope/binding rules + optional pure Lua callbacks |
| **3 — Semantics** | type-aware hover, type diagnostics, semantic completion | declarative type rules over a shared native unification core |
| **4 — Relationships** | call graphs, import graphs, impact radius, call paths | Tier 2/3 output materialised into graph-lite |

Tier 4 is what an agent actually wants: *what calls this, transitively* and *what
breaks if I change this* — answered from a pre-computed graph instead of a hundred
file reads.

## Architecture

```
      editor  ──stdio LSP──┐
      agent   ──MCP────────┤
      CI      ──batch CLI──┘
                           │
                    ┌──────▼──────┐
                    │  shim/cli   │  thin entrypoints, no logic
                    └──────┬──────┘
                           │ unix socket
                  ┌────────▼─────────┐
                  │     daemon       │  one per user, many sessions,
                  │  engine/         │  warm caches, per-workspace state
                  └────┬────────┬────┘
              tree-sitter        │
              + Lua rules        │
                   │             │
              ┌────▼─────┐  ┌────▼─────┐
              │ analysis │──▶ graph-lite│  generic nodes + typed edges
              └──────────┘  └──────────┘  (knows nothing about code)
```

Dependencies flow one way and only one way: `analysis → graph-lite`, never back.
Two lint guards and a standalone-build CI job keep it that way, because the
alternative — noticing at extraction time — is a rewrite.

## Quickstart

```bash
git clone https://github.com/developerz-ai/universal-lsp.git
cd universal-lsp
bin/setup          # prerequisites → configure → link compile_commands.json
bin/check          # the full gate: format, guards, build, tests, sanitizers
bin/dev doctor     # one row per thing that can drift
```

Requires CMake ≥ 3.24, Ninja, a C++20 compiler, clang-format, and
[Bun](https://bun.sh) for the repo tooling. `bin/setup` names anything missing and
how to install it.

## Embedding graph-lite alone

graph-lite builds with no knowledge of this repository — that is asserted on every
PR, so that extracting it later stays a `git mv` rather than a rewrite:

```bash
cmake -S graph-lite -B build/graph-lite -G Ninja
cmake --build build/graph-lite
```

```cpp
#include "graph-lite.h"   // one header, no dependencies
```

API and data model: [`docs/graph-lite/README.md`](docs/graph-lite/README.md).

## Languages at launch

TypeScript · JavaScript · Ruby · C · C++ · Lua · Python · Java · Kotlin · Swift · Rust

Tiers 1 and 2 come first across all eleven — that is most of the value for both
editors and agents. Tier 3 depth grows per language from there, degrading to
"unknown type" rather than to a wrong one.

## Documentation

| Read | For |
|---|---|
| [`docs/spec.md`](docs/spec.md) | the full scope contract — goals, non-goals, build phases |
| [`docs/research/things-to-know.md`](docs/research/things-to-know.md) | the pitfalls already paid for by other people's shipped bugs |
| [`docs/graph-lite/`](docs/graph-lite/README.md) | the library, written as if it already stood alone |
| [`docs/engine/`](docs/engine/README.md) | position encoding, filesystem-change handling, daemon design |
| [`docs/plugins/authoring.md`](docs/plugins/authoring.md) | teaching it a new language |
| [`CLAUDE.md`](CLAUDE.md) | how this repo is worked on — read it before your first change |

## Contributing

Read [`CONTRIBUTING.md`](CONTRIBUTING.md), then [`CLAUDE.md`](CLAUDE.md). Plugins
for languages outside the official eleven need no approval and no place in this
repo — publish a git repository and pin it.

## License

The engine is [Apache-2.0](LICENSE). **graph-lite is [MIT](graph-lite/LICENSE)** and
versioned independently, deliberately: embedding a single header should never
require thinking about the license of the thing that happened to grow it.
