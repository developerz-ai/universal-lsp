# Architecture decision records

One file per decision that has consequences someone will otherwise re-litigate.
Numbered, dated, never deleted — a superseded ADR gets a `Superseded by` line, so
the reasoning survives the reversal.

| # | Decision | Status |
|---|---|---|
| [0001](0001-record-architecture-decisions.md) | Record architecture decisions | Accepted |
| [0002](0002-graph-lite-lives-here-under-mit.md) | graph-lite develops in this repo, under its own license and version | Accepted |
| [0003](0003-no-external-language-servers.md) | No external language-server processes, ever | Accepted |

## When to write one

- A choice with a **named alternative** you are rejecting.
- A **one-way door**: a stored data shape, a public contract, an id scheme.
- Something an agent will otherwise propose again next month, because the reason
  it was rejected is not in the code.

Not for: anything the code already says plainly, or a preference with no
consequence.

## Shape

```markdown
# NNNN — <decision, as a statement>

**Status:** Proposed | Accepted | Superseded by [NNNN](...)
**Date:** YYYY-MM-DD

## Context
What is true that forces a choice. Facts, not narrative.

## Decision
What we will do. Present tense, unambiguous.

## Consequences
What this makes easy, what it makes hard, what it forecloses.

## Alternatives rejected
Each with the reason. An alternative with no stated reason comes back.
```
