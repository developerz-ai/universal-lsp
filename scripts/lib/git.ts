/** Git reads. Writes stay with the human/coordinator — scripts never commit. */
import { UNKNOWN_BASE } from './affected.ts';
import { run } from './cmd.ts';

export { UNKNOWN_BASE };

export const REPO_ROOT = new URL('../..', import.meta.url).pathname.replace(/\/$/, '');

/**
 * Files changed versus the merge base, INCLUDING uncommitted and untracked work.
 * An agent works uncommitted by definition; a detector that only sees commits
 * reports "nothing changed" for the state it is actually being asked about.
 *
 * FAILS CLOSED. If the base ref does not resolve — a first push, where
 * `github.event.before` is all zeros, a shallow clone, a missing remote — this
 * returns `[UNKNOWN_BASE]` rather than an empty list. Empty means "nothing
 * changed", which downstream reads as "skip the build", and CI then reports
 * green over a build it never ran. Under-detection is the dangerous direction:
 * nothing errors.
 */
export async function changedFiles(baseRef = 'origin/main'): Promise<string[]> {
  const merged = new Set<string>();
  const add = (out: string) => {
    for (const line of out.split('\n')) if (line.trim()) merged.add(line.trim());
  };

  const resolvable =
    Boolean(baseRef) &&
    !/^0{7,40}$/.test(baseRef) &&
    (await run(['git', 'rev-parse', '--verify', '--quiet', `${baseRef}^{commit}`], REPO_ROOT))
      .code === 0;

  if (!resolvable) return [UNKNOWN_BASE];

  const base = await run(['git', 'diff', '--name-only', `${baseRef}...`], REPO_ROOT);
  if (base.code !== 0) return [UNKNOWN_BASE];
  add(base.stdout);

  add((await run(['git', 'diff', '--name-only', 'HEAD'], REPO_ROOT)).stdout);
  add((await run(['git', 'ls-files', '--others', '--exclude-standard'], REPO_ROOT)).stdout);

  return [...merged].sort();
}

export async function trackedFiles(globs: string[] = []): Promise<string[]> {
  const res = await run(['git', 'ls-files', ...globs], REPO_ROOT);
  return res.stdout.split('\n').filter(Boolean);
}
