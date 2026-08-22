# 0002 — graph-lite develops in this repo, under its own license and version

**Status:** Accepted
**Date:** 2026-08-22

## Context

graph-lite is intended to be useful to projects that have nothing to do with this
one — it fills a real gap left when the most prominent embeddable graph database
was abandoned in October 2025. But an API designed in the abstract is an API
designed against imagined users. The LSP engine is a demanding, real consumer:
large graphs, interactive latency, constant restarts.

Two failure modes to avoid at once: launching a "1.0 general-purpose database"
before anyone has stressed it, and letting the consumer's vocabulary seep in until
extraction is a rewrite.

## Decision

graph-lite develops **inside this repository**, with its own `LICENSE` (MIT), its
own version number, its own build that configures standalone, and its own test
suite expressed purely in generic node/edge terms. It is not marketed as a stable
standalone product until it has been proven under real usage here.

The extraction decision is spec phase 14 — made on API friction discovered in
phases 1–12, not speculatively up front.

## Consequences

- One repository, one CI, one review flow while the API is still moving.
- Extraction stays cheap **only while the guards stay green**: vocabulary,
  boundary, and the standalone build job. See
  [`../graph-lite-boundary.md`](../graph-lite-boundary.md).
- Two licenses in one repo, which needs saying plainly in the README so an
  embedder is never in doubt: MIT is deliberate, because reasoning about the
  engine's license to vendor one header would be a real adoption tax.

## Alternatives rejected

- **Separate repo from day one** — cross-repo PRs for every API adjustment during
  the exact period when the API changes most.
- **One license for both** — Apache-2.0's NOTICE obligations are friction for an
  embedder vendoring a header; MIT for the engine drops a patent grant worth
  having.
- **Ship graph-lite 1.0 first** — a stable API promised before a demanding
  consumer has touched it is a promise made on guesses.
