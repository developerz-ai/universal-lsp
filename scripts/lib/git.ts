/** Git reads. Writes stay with the human/coordinator — scripts never commit. */
import { run } from './cmd.ts';

export const REPO_ROOT = new URL('../..', import.meta.url).pathname.replace(/\/$/, '');

/**
 * Files changed versus the merge base, INCLUDING uncommitted and untracked work.
 * An agent works uncommitted by definition; a detector that only sees commits
 * reports "nothing changed" for the state it is actually being asked about.
 */
export async function changedFiles(baseRef = 'origin/main'): Promise<string[]> {
  const merged = new Set<string>();
  const add = (out: string) => {
    for (const line of out.split('\n')) if (line.trim()) merged.add(line.trim());
  };

  const base = await run(['git', 'diff', '--name-only', `${baseRef}...`], REPO_ROOT);
  if (base.code === 0) add(base.stdout);

  add((await run(['git', 'diff', '--name-only', 'HEAD'], REPO_ROOT)).stdout);
  add((await run(['git', 'ls-files', '--others', '--exclude-standard'], REPO_ROOT)).stdout);

  return [...merged].sort();
}

export async function trackedFiles(globs: string[] = []): Promise<string[]> {
  const res = await run(['git', 'ls-files', ...globs], REPO_ROOT);
  return res.stdout.split('\n').filter(Boolean);
}
