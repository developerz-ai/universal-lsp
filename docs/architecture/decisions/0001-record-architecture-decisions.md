# 0001 — Record architecture decisions

**Status:** Accepted
**Date:** 2026-08-22

## Context

Agents write the code here and have no memory across sessions. A decision that
lives only in a chat log, a PR comment, or one person's head will be re-proposed —
and the second proposal looks reasonable, because the reason for the first
rejection is nowhere in the repository.

## Decision

Record decisions with consequences as numbered ADRs under
`docs/architecture/decisions/`. Never delete one; supersede it with a new ADR and
link both directions.

## Consequences

- "Why is it like this" is a file, not an excavation of `git log`.
- Rejected alternatives stay rejected with a reason attached, which is the part
  that actually stops the loop.
- Cost: one short file per real decision. Anything the code says plainly does not
  get an ADR.

## Alternatives rejected

- **A single `decisions.md`** — merge conflicts on every parallel change, and no
  stable anchor to link at.
- **Only `git log`** — commit messages describe a change, not a choice, and
  nobody greps them before proposing.
