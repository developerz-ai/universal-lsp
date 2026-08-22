# daemon — thin entrypoints. Wiring only; logic belongs in engine/.
#
#   universal-lsp        the persistent per-user daemon
#   universal-lsp-shim   the stdio shim editors and agent harnesses spawn
#
# Both land in build phase 2/7 (docs/spec.md §14). Until then this file declares
# the shape so the layout question is already answered when the code arrives.
