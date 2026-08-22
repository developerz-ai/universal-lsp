# Persistence

> Design, not shipped behaviour. This is the highest-risk subsystem in the
> library and gets the most testing investment.

## The format follows the layout

The in-memory representation is CSR-style: flat, contiguous arrays. The persisted
format is structurally the same thing. A reload should be close to a read or an
`mmap` plus a header check — not a deserialize-and-rebuild pass. Callers that
restart often (the motivating workload restarts constantly) pay the load cost on
every start, so it has to be near-free.

Consequence to design for: the on-disk layout is a **one-way door** the moment
anything outside a test writes a file. Version the header from the first commit,
and refuse to open a file whose version is newer rather than reading it
optimistically.

## Silent corruption is the failure mode that matters

An embedded store that crashes has told you something. One that returns a plausible
wrong answer has not, and the caller will build on it. So:

- **Checksums over the arrays**, verified on load. A corrupted file must fail to
  open, loudly, naming the offset.
- **Atomic replacement**, not in-place rewriting: write beside, fsync, rename.
- **A file is either the old state or the new state.** Never a third thing.

## Testing investment

Disproportionate, deliberately, relative to the traversal algorithms — the
algorithms fail visibly, this fails invisibly.

| Test | Asserts |
|---|---|
| Reader fuzzing | No malformed or truncated input can crash, hang, or read out of bounds. |
| Truncation sweep | Cut the file at every offset; every result is a clean refusal. |
| Crash consistency | Interrupt a write at every offset; reopen; state is old-or-new. |
| Round-trip properties | Save/load of a generated store preserves every node, edge, property and id. |
| Version refusal | A file stamped with a newer format version is refused, not guessed at. |
