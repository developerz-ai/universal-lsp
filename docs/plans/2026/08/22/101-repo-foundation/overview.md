# Repository foundation

## Goal

Get this repository ready to be worked on by agents: build system, three-tier
gate, CI, mechanical guards for the one rule that cannot be enforced by review,
and the documentation set. No engine implementation — the point is that phase 1
can start with nothing left to decide about tooling.

## Context

- Stack: C++20, CMake ≥ 3.24 + Ninja (presets only), GoogleTest + CTest, Bun-TS
  for repo automation under `scripts/`.
- Conventions inherited from `../gold-standards-in-ai/docs/` (a sibling checkout,
  not vendored).
- Scope contract: [`docs/spec.md`](../../../../../spec.md). Pitfalls already paid
  for: [`docs/research/things-to-know.md`](../../../../../research/things-to-know.md).
- The one rule that needs mechanising: dependencies flow `analysis → graph-lite`,
  never back, and graph-lite never learns the consumer's vocabulary.

## Plan files

1. [01-build-system.md](01-build-system.md) — CMake, presets, sanitizers, targets
2. [02-gate-and-guards.md](02-gate-and-guards.md) — the three tiers, the change detector, the lint guards
3. [03-ci.md](03-ci.md) — workflows, parity, the aggregate check
4. [04-docs.md](04-docs.md) — the documentation set and its index
5. [05-agent-config.md](05-agent-config.md) — `CLAUDE.md`, `.claude/`, `.mcp.json`

## Done when

- `bin/setup` takes a fresh clone to a configured build tree with no manual steps.
- `bin/check` is green and mirrors CI exactly, asserted by a test.
- A vocabulary or boundary violation in `graph-lite/` fails the gate, and each
  guard has its own test proving it flags a bad sample and passes a good one.
- `cmake -S graph-lite` builds with no knowledge of the parent repo, in CI.
- Every doc referenced from `README.md`, `CLAUDE.md` and `docs/README.md` exists.

## Risks / open questions

- **Runners.** CI is on `ubuntu-latest`, not the house Blacksmith standard,
  because this repo is not enrolled and an un-enrolled label yields *absent*
  checks rather than red ones. Revisit on enrolment; `ci-parity.test.ts` pins it
  so the change is deliberate.
- **GoogleTest fetch.** Pinned, with a system-package fallback, so an offline box
  and CI both work. Revisit if the fetch becomes the slowest part of a cold CI run.
- **`clang-tidy` is not installed on this dev box.** CI runs it regardless;
  `bin/dev doctor` reports it as an optional row rather than failing.
