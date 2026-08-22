# Filesystem changes

## There are two paths, not three

It is tempting to model "keystrokes", "bulk external changes" and "agent batch
edits" as three cases. They are two, and the split is not where instinct puts it:

1. **Interactive `didChange` editing** — only happens when a live editor client is
   attached. Small, frequent, single-file, rich protocol signal.
2. **Filesystem bursts** — `git pull`, a formatter, a build, and **an AI agent
   writing files directly to disk**. No protocol signal at all: just raw watcher
   events, indistinguishable from each other.

Path 2 is the *default* case for this project's primary user. Any design that
assumes edits arrive as `didChange` notifications simply does not run for the
client we are building for.

## Path 1 — interactive edits

- Incremental reparse of the changed region; shallow, fast reanalysis.
- Defer full re-binding / re-typing / re-indexing to save or a short idle period.
- **Run a full fresh reparse periodically** — on a cadence, after N incremental
  edits, or whenever an incrementally-produced tree contains unexpected error
  nodes. Incremental reparse is *not* guaranteed to converge to the same tree as
  parsing the final text from scratch; spurious error nodes surviving an edit is
  documented, reproduced behaviour in tree-sitter, not a hypothetical.

## Path 2 — filesystem bursts

```
watcher ──▶ debounce ──▶ coalesce ──▶ content-hash diff ──▶ one reindex job
(inotify/  (2 params)   (one batch)   (not mtime)          (priority-split)
 FSEvents/
 kqueue)
```

- **A single logical save is not a single filesystem event.** A raw watcher emits
  a burst of low-level operations for what a person calls "one save". Interpreting
  bursts as one logical change is the debounce layer's entire job.
- **Two debounce parameters, not one window**: a maximum wait for the first event
  (bounds worst-case latency) plus a *quiet period* that resets on each new event
  (coalesces genuine bursts). This is what mature watcher libraries converged on.
- **Configurable, not hardcoded.** Practice clusters in 100ms–2s: diagnostics-
  oriented tooling near ~150ms, reindex-oriented nearer 1–2s. "How snappy should
  diagnostics feel" versus "how much redundant work do we tolerate" is a real
  tuning trade, and diagnostics may deserve their own shorter window.
- **Diff by content hash, not mtime.** Hashes distinguish a rename or move from a
  delete+create pair; mtime cannot.
- **Exclude ignorable directories from the watch scope** — build output,
  dependency directories, VCS internals — sourced from the same project-manifest
  awareness used for import resolution. Recursive watching consumes per-entry
  kernel resources and hits platform limits; filtering after the fact still pays.
- **Priority split.** Currently open/visible files reanalyse immediately;
  everything else queues at background priority. rust-analyzer had to retrofit
  exactly this deprioritization after naive immediate reindexing on agent-driven
  changes made inlay hints visibly jump and go stale.

## The settle request

The engine exposes an explicit **"wait for idle"** request — a custom LSP
extension and an equivalent MCP tool — that resolves once debouncing and
reanalysis for the caller's batch of edits have completed.

This exists because the alternative is an agent sleeping for an arbitrary duration
and hoping. A deterministic signal that "diagnostics now reflect my edits" is the
difference between a reliable agent loop and a flaky one.
