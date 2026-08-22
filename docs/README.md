# docs/

Everything written about this project that is not code. The layout mirrors the
repository's internal boundaries on purpose — including
[`graph-lite/`](graph-lite/README.md), which reads as though the library already
stood alone, because that is what catches a leak before it becomes a rewrite.

## Start here

| If you want to | Read |
|---|---|
| know what is in scope, and what is deliberately not | [`spec.md`](spec.md) |
| design a subsystem without repeating a known mistake | [`research/things-to-know.md`](research/things-to-know.md) |
| work in this repo | [`../CLAUDE.md`](../CLAUDE.md) |
| know why something is the way it is | [`architecture/decisions/`](architecture/decisions/README.md) |
| debug a symptom | [`gotchas.md`](gotchas.md) |

## The subsystems

| Dir | Scope |
|---|---|
| [`graph-lite/`](graph-lite/README.md) | The embeddable graph library: [data model](graph-lite/data-model.md), [traversal](graph-lite/traversal.md), [persistence](graph-lite/persistence.md). No consumer vocabulary, enforced. |
| [`engine/`](engine/README.md) | Daemon core: [position encoding](engine/position-encoding.md), [filesystem changes](engine/filesystem-changes.md), [daemon + shim](engine/daemon.md), [concurrency](engine/concurrency.md). |
| [`analysis/`](analysis/README.md) | The translation layer — the only place both vocabularies meet. |
| [`plugins/`](plugins/README.md) | The Lua plugin system and [how to author one](plugins/authoring.md). |

## Cross-cutting

| Dir / file | Scope |
|---|---|
| [`architecture/`](architecture/README.md) | [The graph-lite boundary](architecture/graph-lite-boundary.md), [the four tiers](architecture/tiers.md), [ADRs](architecture/decisions/README.md). |
| [`standards/`](standards/testing.md) | [Testing](standards/testing.md), [C++ conventions](standards/code-style.md), [the dev loop](standards/dev-loop.md). |
| [`research/`](research/things-to-know.md) | Prior art and pitfalls, gathered before implementation. Dated. |
| [`gotchas.md`](gotchas.md) | Symptom → cause → fix. Read when something is already wrong. |
| [`plans/`](plans/README.md) | `/planx` output — dated, self-contained execution plans. |

## House rules for docs here

- **Lead with the rule, not the reason.** Fragments over sentences, tables over
  paragraphs. Paths and commands stay verbatim.
- **Reference, don't paste.** Point at `path:line`; copied code is stale on
  arrival.
- **Date load-bearing claims** (`As of 2026-08`). Delete a stale doc rather than
  letting it rot — a wrong doc costs more than a missing one.
- **Gap statements rot fastest.** "We haven't done X yet" is wrong the day someone
  does it. Write "when you do X, follow `path`" instead.
- Preventive rules go in [`../CLAUDE.md`](../CLAUDE.md) (read every session);
  symptom lookups go in [`gotchas.md`](gotchas.md) (read when something breaks).
