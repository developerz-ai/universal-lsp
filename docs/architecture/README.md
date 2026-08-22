# Architecture

Cross-cutting design that belongs to neither subsystem alone.

| Doc | Scope |
|---|---|
| [graph-lite-boundary.md](graph-lite-boundary.md) | The one-way dependency rule, why it is mechanical, and what breaks it |
| [tiers.md](tiers.md) | The four capability tiers and what each is allowed to assume |
| [decisions/](decisions/README.md) | ADRs — decisions with consequences, and the ones deliberately rejected |

Subsystem-local design lives with its subsystem: [`../engine/`](../engine/README.md),
[`../graph-lite/`](../graph-lite/README.md), [`../analysis/`](../analysis/README.md),
[`../plugins/`](../plugins/README.md).
