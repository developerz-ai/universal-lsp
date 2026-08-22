# The graph-lite boundary

**Dependencies flow one way: `analysis` → `graph-lite`. Never back.**

graph-lite must never reference an LSP concept — symbol, scope, document, hover,
diagnostic, plugin, tree-sitter — in its public API, its internals, its tests, or
its documentation. Those concepts are translated into generic nodes and typed
edges by the `analysis/` layer, which is the only place in the repository where
both vocabularies are allowed to meet.

## Why this is mechanical rather than a convention

The library's entire value is that it is generic, and its first consumer is a
large, opinionated system that will push vocabulary downward one innocuous
identifier at a time. `SymbolNode` looks harmless. So does a doc paragraph
explaining what a node "usually" is. By the time anyone notices, extracting the
library is a rewrite instead of a `git mv` — and the extraction decision (spec
phase 14) is supposed to be made on API friction, not on how much cleanup it
would cost.

Review discipline does not catch this. Three mechanisms do:

| Mechanism | Catches |
|---|---|
| `scripts/lint/graph-lite-vocabulary.ts` | a borrowed concept name in the library, its tests, or its docs — **tokenising camelCase and snake_case**, because `\bsymbol\b` does not match `SymbolNode` and `SymbolNode` is exactly the shape the leak takes |
| `scripts/lint/graph-lite-boundary.ts` | an `#include` of a consumer header, or a `target_link_libraries` against a repo target — both of which compile perfectly |
| CI job `standalone` | anything that makes `cmake -S graph-lite` stop working, on every PR |

Each guard has its own test asserting it flags a bad sample *and* passes a good
one. A guard that silently matches nothing is worse than no guard, because it
looks green.

## What is scanned, and what is not

Scanned — everything that ships when the library is extracted:
`graph-lite/include/**`, `graph-lite/src/**`, `graph-lite/README.md`,
`tests/graph-lite/**`, `docs/graph-lite/**`.

Not scanned — repo-side glue that stays behind: `graph-lite/CMakeLists.txt` (it
links this repo's shared warning/sanitizer interface targets when built as a
subdirectory, and defines its own when standalone) and `graph-lite/CLAUDE.md`.

This document lives here, not under `docs/graph-lite/`, precisely because it
cannot be written without the vocabulary it forbids.

## The generic words are not banned

`scope`, `reference`, `node`, `file` stay allowed. A guard with false positives
gets disabled, and a disabled guard is worse than none. The banned list is
concept names that could only have come from the consumer.

## If you need to violate it

You do not. The translation belongs in `analysis/`: pass the data in, or move the
logic out. If that genuinely cannot be done, the API is wrong in a way worth an
ADR — not an allowlist entry.
