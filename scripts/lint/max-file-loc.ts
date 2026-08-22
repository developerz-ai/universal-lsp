/**
 * Files stay under 500 lines. Not style: a 200-line file is one read with full
 * context, a 2000-line file is chunked reads and lost context — the agent pays
 * that on every visit. Split by responsibility (SRP), don't raise the ceiling.
 */
import { Glob } from 'bun';
import { REPO_ROOT } from '../lib/git.ts';
import type { Finding } from '../lib/guard.ts';

const LIMIT = 500;
const SCAN = '**/*.{cpp,h,hpp,c,lua,ts}';
const SKIP = [/^build\//, /^node_modules\//, /^tmp\//, /^\.git\//];

export const name = 'max-file-loc';
export const describe = `every source file is under ${LIMIT} lines`;

export async function run(root: string = REPO_ROOT): Promise<Finding[]> {
  const findings: Finding[] = [];

  for await (const rel of new Glob(SCAN).scan({ cwd: root })) {
    if (SKIP.some((re) => re.test(rel))) continue;
    const count = (await Bun.file(`${root}/${rel}`).text()).split('\n').length;
    if (count <= LIMIT) continue;
    findings.push({
      file: rel,
      line: LIMIT,
      message:
        `${count} lines, limit ${LIMIT}. Split by responsibility — one file, one job ` +
        `(docs/standards/code-style.md). Guard: scripts/lint/max-file-loc.ts`,
    });
  }

  return findings;
}
