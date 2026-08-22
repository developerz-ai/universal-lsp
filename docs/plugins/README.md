# plugins/

One directory per officially supported language, each containing that language's
Lua rule files. Community plugins live **outside** this repository by design —
they need no approval and no place here.

> Pre-implementation. The plugin system is build phase 3; the official set is
> phase 11.

| Doc | For |
|---|---|
| [authoring.md](authoring.md) | writing a plugin for a new language |

## Launch languages

TypeScript · JavaScript · Ruby · C · C++ · Lua · Python · Java · Kotlin · Swift · Rust

Tiers 1–2 across all eleven first; Tier 3 depth grows per language from there.
Lua, C and JavaScript/TypeScript are the early candidates for proving the
declarative rule system — their lexical scoping is the most regular.

## Distribution

- Plugins are git repositories, installed by URL, **pinned to a commit or tag**.
- A project declares its plugins and pins in a checked-in manifest.
- Because no plugin depends on an external tool, a project's tooling is fully
  reproducible from those pins alone.
- A curated short-name index may come later, purely for discovery. It is not a
  gate and never becomes one.
