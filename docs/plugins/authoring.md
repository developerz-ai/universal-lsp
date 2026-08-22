# Writing a language plugin

> Pre-implementation — the rule schema below is the design, not a shipped
> contract. It will move before phase 3 lands. Do not build against it yet.

A plugin is a self-contained unit written **entirely in Lua**, declaring support
for one or more languages by file extension or file type.

## What a plugin may never do

- Spawn a process.
- Open a network connection.
- Depend on an external binary.

Everything is declarative rule tables plus small pure callbacks that the engine
evaluates natively. This is not a sandbox policy, it is the product: a language's
capability must never depend on what is installed on the machine.

## What a plugin declares, per tier

| Tier | Declares |
|---|---|
| 1 | a tree-sitter grammar reference, plus query files for highlighting, folding and symbol extraction |
| 2 | which node types open scopes, bind names, or are references — plus optional pure Lua callbacks for the irregular cases (imports, macros, dynamic scoping quirks) |
| 3 | how literals, expressions and calls produce and constrain types, in terms the engine's native unification core understands — plus callbacks for constructs too irregular to declare |
| — | static completion sources (keywords, snippets) and optional formatting rules |

Plus a **minimum engine version**, so an incompatible pairing is detected and
reported rather than behaving strangely.

## Rules for callbacks

- **Pure.** Same input, same output. No I/O, no globals, no clock.
- **Budgeted.** Each invocation gets an instruction-count or time budget. A
  runaway callback must not be able to hang or unboundedly grow the daemon —
  a budget is sufficient for that; a full sandboxed VM is not needed.
- **Small.** A callback is an escape hatch for an irregular construct. If most of
  a language is expressed in callbacks, the declarative schema is missing
  something — say so, rather than working around it.

## Coverage is allowed to be partial

Start with the common, everyday subset of a language and expand. An unsupported
construct must degrade to **"unknown"**, never to a guess: an agent acts on a
confident wrong answer, and a wrong type is worse than an absent one.

## Bringing your own grammar

tree-sitter grammars are authored in JavaScript and compiled to C. That is a
**plugin-build-time** step, entirely separate from the Lua configuration the
engine reads at runtime — the engine consumes the compiled grammar and never sees
the JS toolchain.

## Testing a plugin

Golden-file tests under `tests/plugins/<language>/`: feed a sample file, assert
the resulting scope tree, highlight ranges and diagnostics equal a checked-in
expected output. A plugin change that alters output alters a golden file, visibly,
in the diff — which is the entire point.
