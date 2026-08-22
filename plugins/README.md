# plugins/

One directory per officially supported language, each a self-contained set of Lua
rule files. Empty until build phase 3 — the plugin system lands there, the
official eleven in phase 11.

Community plugins do **not** belong here. A plugin is a git repository installed
by URL and pinned to a commit; no approval and no place in this repo is needed.

How to write one: [`../docs/plugins/authoring.md`](../docs/plugins/authoring.md).
