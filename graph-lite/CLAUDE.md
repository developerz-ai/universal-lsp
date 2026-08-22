# graph-lite — working rules

Repo-side context for this directory. Not scanned by the vocabulary guard, and not
part of what ships if the library is extracted — which is exactly why the "why"
lives here instead of in the header.

## The rule

**Nothing from the consumer may appear here.** No LSP concept — symbol, scope,
document, hover, diagnostic, plugin, tree-sitter — in the API, internals, tests,
or `docs/graph-lite/`. No `#include` of an engine header. No link against a repo
target.

The translation from those concepts to typed nodes and edges lives in
`analysis/`. Full reasoning and the enforcement mechanisms:
[`../docs/architecture/graph-lite-boundary.md`](../docs/architecture/graph-lite-boundary.md).

## What the guards will tell you

| Guard | Fires on |
|---|---|
| `scripts/lint/graph-lite-vocabulary.ts` | a borrowed concept name — including inside `CamelCase` and `snake_case` identifiers |
| `scripts/lint/graph-lite-boundary.ts` | a consumer `#include`, or a `target_link_libraries` against a repo target |
| CI job `standalone` | anything that breaks `cmake -S graph-lite` |

When one fires, the fix is to invert the dependency or move the logic — never an
allowlist entry, and never a looser rule.

## Layout

```
include/graph-lite.h    the ONE public header — additions here are a public contract
src/                    implementation
CMakeLists.txt          configures standalone AND as a subdirectory
CMakePresets.json       standalone preset; ships on extraction
LICENSE                 MIT, deliberately not the engine's license
```

## Conventions

- **The header is a one-way door.** Anything added to `graph-lite.h` is a promise:
  node ids, the persisted layout, and every exported signature. Think before, not
  after.
- Version this library independently — `GRAPH_LITE_VERSION` in `CMakeLists.txt`
  and the macros in the header move together, and neither tracks the engine.
- Tests live in `tests/graph-lite/`, expressed purely in the generic model. If a
  test needs borrowed vocabulary to describe itself, the API leaked.
- Persistence changes need a format version bump and a refusal path for a newer
  file. See [`../docs/graph-lite/persistence.md`](../docs/graph-lite/persistence.md).

## Commands

```bash
cmake -S graph-lite -B build/graph-lite-standalone -G Ninja   # the extraction proof
cmake --build build/graph-lite-standalone
bin/dev test graph_lite                                        # this library's suite only
```
