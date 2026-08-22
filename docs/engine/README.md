# engine/

The LSP daemon core: transport, session and workspace management, worker pools,
the scheduler, the filesystem-watch pipeline, Lua embedding, and the MCP
front-end. Depends on graph-lite; graph-lite never depends back.

> Pre-implementation. These documents are the design being built. Build phases in
> [`../spec.md`](../spec.md) §14.

| Doc | Read before |
|---|---|
| [position-encoding.md](position-encoding.md) | touching offsets, ranges, or anything that indexes text |
| [filesystem-changes.md](filesystem-changes.md) | the watcher, debounce, or reindex scheduling |
| [daemon.md](daemon.md) | the process model, the shim, session multiplexing |
| [concurrency.md](concurrency.md) | adding a thread, a queue, or shared state |

## Shape

```
I/O thread          transport decode/encode ONLY — no parsing, no analysis
  │
  ├─ interactive pool   open/focused documents. Latency budget. One document is
  │                     always routed to the same worker, so its syntax tree
  │                     needs no cross-thread lock.
  └─ background pool    whole-workspace indexing. Lower priority. Cancellable,
                        and superseded the moment its file becomes interactive.
```

Pool sizing is tuned around CPU cost — parsing, scope resolution, type solving —
because there are no subprocesses to wait on. That is a direct consequence of the
no-external-processes rule, and it inverts the usual language-server sizing
instinct.

## Interfaces

| Surface | Transport | For |
|---|---|---|
| LSP | stdio | editors, and agent harnesses that speak raw LSP |
| MCP | the daemon socket | agents — batch/composite questions, backed by the relationship graph |
| batch CLI | one process, JSON out | CI, and the simplest possible integrations |

MCP is a co-equal front-end, not a wrapper bolted on afterwards. An agent asking
"what breaks if I change this" should get one answer from one call, not compose
forty LSP requests to derive it.
