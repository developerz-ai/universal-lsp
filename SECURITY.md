# Security policy

## Reporting a vulnerability

**Do not open a public issue.** Use GitHub's private vulnerability reporting on
this repository (Security → Report a vulnerability), which reaches the maintainers
directly.

Include: what you did, what happened, what you expected, and the smallest input
that reproduces it. A file that triggers it is worth more than a paragraph
describing one.

Expect an acknowledgement within a few days. We will tell you what we found, what
we are doing, and when — and credit you in the release notes unless you would
rather we did not.

## Scope

This project's threat model is **local input, not remote traffic**: the engine
reads source files, plugin rule files, and persisted index data from the machine
it runs on, and it is deliberately incapable of making network calls.

Most interesting to us:

| Area | Why it matters |
|---|---|
| Persistence (`graph-lite`) | A malformed or truncated store must be refused, never read as plausible-but-wrong data. Silent corruption is the worst failure mode here. |
| Protocol decoding | Untrusted JSON-RPC framing and offsets. An out-of-bounds read from a crafted `Position` is in scope. |
| Text handling | Offset arithmetic on arbitrary UTF-8, including astral-plane and malformed sequences. |
| Lua plugin execution | A plugin is code, and installing one is a trust decision. But a plugin must not be able to hang the daemon indefinitely, or reach a process or the network — if it can, that is a bug in the budget or the embedding. |
| The daemon socket | A local socket, never a network port. Anything that changes that is a vulnerability, not a feature. |

Out of scope: a plugin you deliberately installed doing something you told it to;
resource exhaustion from a genuinely enormous input where the engine degrades
loudly rather than silently.

## Supported versions

Pre-1.0. Only the current `main` is supported. Once releases exist, this table
will name them.
