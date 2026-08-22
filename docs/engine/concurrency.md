# Concurrency

## Thread roles

| Thread / pool | Owns | Must never |
|---|---|---|
| I/O thread | transport decode/encode | parse, analyse, or take an analysis lock |
| Interactive pool | open/focused documents | be blocked by background work |
| Background pool | whole-workspace indexing | outrank an interactive request |

An open document is consistently routed to the **same** interactive worker, so its
syntax tree needs no cross-thread lock — routing is the synchronisation.

## Shared state

Favour reader-friendly structures over broad locks: sharded maps, snapshots,
copy-on-write. The requirement this satisfies is concrete — background reindexing
must not stall an interactive query, and a single mutex over the symbol index
guarantees it will.

## Cancellation is a requirement, not a feature

An in-flight background job for a file must be **cancellable and superseded
immediately** when that file becomes interactively relevant. Without it the
priority split is decorative: the interactive request still waits for a background
job that is already wrong.

## Sizing

Tune pools around CPU-bound cost — parsing, scope resolution, type solving. There
are no subprocesses to wait on, so the usual "oversubscribe for I/O wait" instinct
produces contention rather than throughput.

## Budgets that degrade, not hang

Type solving has pathological cases: naive Hindley-Milner-style inference can hit
exponential behaviour on tuple/record-heavy code or deeply nested polymorphic
let-bindings. A per-file or per-expression step/time budget that degrades to
"unknown type" is mandatory — an interactive worker thread that hangs is worse
than an answer that admits it does not know.

Same shape for the occurs check: unifying a type variable with a type containing
itself must be detected and rejected. Omitting it does not produce a wrong answer,
it produces infinite recursion — the symptom is "the editor stopped responding".
