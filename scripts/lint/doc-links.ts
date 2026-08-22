/**
 * Every relative link in a markdown file resolves to a real path.
 *
 * Docs rot silently: an index keeps listing a file someone renamed, and the next
 * agent concludes the documentation is unreliable and stops reading it. A dead
 * link is the cheapest possible detection of that, and it costs nothing to run.
 *
 * Not checked: external URLs (network in the gate is a flake source), pure
 * anchors, and anything inside a fenced code block — a link template in an
 * example is not a claim that the path exists.
 */

import { relative, resolve } from 'node:path';
import { Glob } from 'bun';

import { REPO_ROOT } from '../lib/git.ts';
import { type Finding, lines } from '../lib/guard.ts';

const SKIP_DIR = /^(build|node_modules|tmp|\.git)\//;
const LINK = /\[[^\]]*\]\(([^)]+)\)/g;

export const name = 'doc-links';
export const describe = 'every relative markdown link resolves';

function isExternal(target: string): boolean {
  return /^([a-z]+:)?\/\//.test(target) || target.startsWith('#') || target.startsWith('mailto:');
}

export async function run(root: string = REPO_ROOT): Promise<Finding[]> {
  const findings: Finding[] = [];

  for await (const rel of new Glob('**/*.md').scan({ cwd: root })) {
    if (SKIP_DIR.test(rel)) continue;
    const dir = rel.includes('/') ? rel.slice(0, rel.lastIndexOf('/')) : '';

    let inFence = false;

    (await lines(`${root}/${rel}`)).forEach((text, i) => {
      if (text.trimStart().startsWith('```')) {
        inFence = !inFence;
        return;
      }
      if (inFence) return;

      for (const match of text.matchAll(LINK)) {
        const raw = (match[1] ?? '').trim();
        if (!raw || isExternal(raw)) continue;

        const target = (raw.split('#')[0] ?? '').replace(/^\.\//, '');
        if (!target) continue;

        const resolved = target.startsWith('/')
          ? resolve(root, `.${target}`)
          : resolve(root, dir, target);

        // Bun.file().size is 0 for a directory as well as a missing file, so ask
        // the filesystem about both shapes rather than trusting one probe.
        const exists =
          Bun.file(resolved).size > 0 ||
          Bun.file(`${resolved.replace(/\/$/, '')}/README.md`).size > 0;

        if (exists) continue;
        findings.push({
          file: rel,
          line: i + 1,
          message:
            `dead link "${raw}" → ${relative(root, resolved)} does not exist. ` +
            `Fix the path or delete the link — a stale index teaches the next reader ` +
            `to stop trusting the docs. Guard: scripts/lint/doc-links.ts`,
        });
      }
    });
  }

  return findings;
}
