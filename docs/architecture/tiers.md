# The four tiers

A language gets exactly the depth its plugin's rules describe. Never more, never
less, and **never as a function of what is installed on the machine** — that
independence is the product.

| Tier | Capability | Engine machinery | Plugin declares |
|---|---|---|---|
| 1 — Syntax | highlighting, folding, outline, bracket matching | tree-sitter | grammar reference + query files |
| 2 — Structure | definition, references, rename, workspace symbols | native scope-resolution algorithm | which node types open scopes, bind names, are references (+ optional pure Lua callbacks) |
| 3 — Semantics | typed hover, type diagnostics, semantic completion | native constraint generation + union-find unification | declarative type rules (+ callbacks for irregular constructs) |
| 4 — Relationships | call graph, imports, impact radius, call paths | the `analysis` translation layer over graph-lite | nothing extra — falls out of Tier 2/3 output |

## Rules that apply across tiers

- **Degrade to unknown, never to wrong.** A Tier 3 construct the rules do not
  cover yields "unknown type". A confidently wrong type is worse than no type: an
  agent will act on it.
- **Tier 1 is nearly free** for any language with an existing tree-sitter grammar.
  Tiers 1–2 across all eleven launch languages come before Tier 3 depth in any of
  them — navigation and structure are most of the value for both editors and
  agents.
- **Tier 4 is the reason agents care.** "What calls this, transitively" and "what
  breaks if I change this" are answered from a pre-computed graph rather than
  re-derived by a hundred file reads.
- **tree-sitter is not a diagnostic engine.** Its error recovery is effectively a
  black box, and `MISSING` nodes are hard to introspect. Use its error nodes for
  "something is wrong in this range" and nothing more; Tier 2/3 generate their own
  precise messages.

## Tier 3 is the risky one

It is the most novel subsystem in the project. Validate it end to end on one
simple, regular language before expanding — and design the constraint
representation to retain *why* a constraint was generated, not just the final
substitution. That traceability is what turns "type mismatch" into "this call
requires X because of the earlier argument at line N", and retrofitting it later
is expensive.
