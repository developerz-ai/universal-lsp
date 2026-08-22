# Contributing

This repo is **AI-first**: agents write essentially all of the code, humans review
and steer. Everything below optimises for that — which also makes it a pleasant
repo for a human, because the same things matter (exact commands, fast feedback,
mechanical rules).

Read [`CLAUDE.md`](CLAUDE.md) before your first change. It is the contract.

## Prerequisites

| Tool | Why |
|---|---|
| CMake ≥ 3.24 | presets |
| Ninja | the generator every preset uses |
| A C++20 compiler | GCC 13+ or Clang 16+ |
| clang-format | the format gate |
| [Bun](https://bun.sh) | the repo automation under `scripts/` |
| clang-tidy *(optional)* | local static analysis; CI runs it regardless |

## Getting started

```bash
bin/setup          # prerequisites → configure build/dev → link compile_commands.json
bin/check          # the full gate — expect ~1 minute
bin/dev doctor     # one row per thing that can drift
bun run scripts/help.ts
```

`bin/setup` names anything missing and how to install it. If a step needs
knowledge that is not in the repo, that is a bug in our DX — file it.

## The loop

| After | Run | Cost |
|---|---|---|
| each edit | `bin/check --files <paths>` | < 3s |
| a slice compiles | `bin/check --changed` | < 30s |
| before pushing | `bin/check` | ~1 min |

Do **not** reflexively run the full gate — it configures three build trees and
runs the suite twice, once under ASan + UBSan. Details and blind spots:
[`docs/standards/dev-loop.md`](docs/standards/dev-loop.md).

## Tests are not optional

TDD is the working method: failing test, then code. A bug fix starts with a
reproducing test; a refactor is green before and after. There is no UI to eyeball
and no staging to poke here — a test is the only thing that says "done".
Conventions: [`docs/standards/testing.md`](docs/standards/testing.md).

## The one rule that is enforced mechanically

**Dependencies flow `analysis → graph-lite`, never back**, and graph-lite never
learns the consumer's vocabulary. Two lint guards and a standalone-build CI job
enforce it. If a guard flags you, the translation belongs in `analysis/` — not in
an allowlist entry. Background:
[`docs/architecture/graph-lite-boundary.md`](docs/architecture/graph-lite-boundary.md).

## Pull requests

- Branch `<type>/<slug>`; conventional commits (`feat:`, `fix:`, `docs:`, …).
- One concern per PR, ~110 changed files max — automated review skips past a
  limit, so an oversized PR gets the *least* review.
- `Closes #N` in the body.
- **Never push red.** A PR with 0 checks registered is not a passing PR; wait for
  them to attach.
- Ship the guard with the fix: a defect that can recur gets its test or lint rule
  in the same PR.

## Adding a language

You do not need this repository. A plugin is a git repo of Lua rule files,
installed by URL and pinned to a commit — no approval, no central build step.
See [`docs/plugins/authoring.md`](docs/plugins/authoring.md). The eleven official
plugins live here only because they ship with the engine.

## Reporting

Bugs and features: the [issue templates](.github/ISSUE_TEMPLATE). Security issues:
**do not** open an issue — see [`SECURITY.md`](SECURITY.md).
