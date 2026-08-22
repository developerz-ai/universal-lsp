# Daemon and shim

## One process, many sessions

The engine runs as a **single persistent daemon per user**, not a fresh process
per editor window or per invocation. Four editor windows and three agent sessions
across two projects share one process, one set of warm caches, one index.

Editors and agent harnesses expect to spawn a stdio child. They get a small
**shim** binary that makes that true from the outside:

```
editor spawns shim
   └─ connect to the well-known local socket        (unix socket / named pipe;
        ├─ present  → forward bytes both ways        NEVER a network port)
        └─ absent   → spawn daemon (lock/pidfile so
                      a race cannot start two), wait
                      for the socket, then connect
```

A dead or version-mismatched daemon is detected at handshake, and the shim
respawns and reconnects — the editor sees an ordinary server restart.

## Session and workspace state

- Each session gets its own logical handshake and open-document set.
- Sessions on the **same project root share one workspace analysis state** —
  symbol tables, type caches, the relationship graph — reference-counted, and kept
  warm for a grace period after the last session disconnects in case one comes
  back.
- Workspace states for unrelated projects are fully independent: no cross-project
  locking, and a memory budget with LRU eviction so one large project cannot
  starve the others sharing the daemon.
- Multi-root (`workspaceFolders`) is supported from the start — it is native to
  the spec and it is what the daemon model already needs.

## Scheduling: FIFO, deliberately

Request scheduling within each worker pool is plain first-in-first-out, with no
cross-session priority or fairness logic.

**This is a documented, accepted simplification, not an oversight.** A large burst
from one aggressive agent session *can* measurably delay another session's
interactive request, including a human's. The cheap refinement, if real usage
demands it, is a two-queue split — currently-focused-document requests versus
everything else — not a multi-level fairness scheduler.
