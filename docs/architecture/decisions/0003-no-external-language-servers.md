# 0003 — No external language-server processes, ever

**Status:** Accepted
**Date:** 2026-08-22

## Context

The obvious way to support eleven languages quickly is to shell out: proxy to
rust-analyzer, pyright, clangd, and translate. It works on the demo machine.

It also means every user must install eleven toolchains; memory is the sum of
eleven servers; behaviour depends on which versions happen to be on PATH; and the
relationship graph can only contain what each server chooses to expose. For the
primary target — an agent in a container, in CI, on someone else's laptop — "it
depends what is installed" is the failure, not a caveat.

## Decision

The engine never spawns, proxies to, or depends on an external language server,
subprocess, or network service as part of normal operation. Every tier is
implemented natively and configured declaratively in Lua.

Capability degrades as a function of **how complete a plugin's rules are**, never
as a function of what is installed.

## Consequences

- Self-containment is testable, and is tested: the suite must pass in a sandbox
  with no other language toolchains present.
- Worker pools are sized for CPU cost, not I/O wait — there is nothing to wait on.
- Tier 3 for C++ templates, Rust traits and Swift protocol conformance is
  genuinely hard and will take a long time. Accepted: those degrade to "unknown
  type", which is honest, rather than to "wrong type", which is not.
- A "temporary" proxy would become the architecture. There is no version of this
  we ship behind a flag.

## Alternatives rejected

- **Proxy now, replace later** — the replacement never happens; the proxy becomes
  the product and the differentiator is gone.
- **Proxy as an optional fallback** — two code paths, one of which is untested by
  construction, and users would rely on the one we do not control.
