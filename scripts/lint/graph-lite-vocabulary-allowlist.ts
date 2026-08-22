/**
 * Pre-existing violations, by repo-relative path. Shrinks over time; never grows
 * to accommodate new code. Weakening the rule to fit a violation is not an
 * option — the rule is the asset.
 *
 * Empty, and the intent is that it stays empty.
 */
export const ALLOWLIST: string[] = [];
