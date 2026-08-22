#!/usr/bin/env bun
/**
 * Runs every repo guard. Wired into `bin/check` AND into CI's own job — a guard
 * whose subject is the whole tree must never have its execution gated on a
 * subset of the tree, or the required check goes green over the one diff shape
 * that breaks it.
 *
 * Guards are DISCOVERED from this directory, not listed: a registry is a file
 * somebody has to remember to update.
 */
import { Glob } from 'bun';
import { REPO_ROOT } from '../lib/git.ts';
import { type Finding, formatFinding } from '../lib/guard.ts';
import { fail, ok, step } from '../lib/log.ts';

interface LoadedGuard {
  name: string;
  describe: string;
  run: () => Promise<Finding[]>;
}

async function loadGuards(): Promise<LoadedGuard[]> {
  const guards: LoadedGuard[] = [];
  for await (const rel of new Glob('*.ts').scan({ cwd: `${REPO_ROOT}/scripts/lint` })) {
    if (rel === 'index.ts' || rel.endsWith('.test.ts') || rel.endsWith('-allowlist.ts')) continue;
    const mod = await import(`${REPO_ROOT}/scripts/lint/${rel}`);
    if (typeof mod.run !== 'function') {
      throw new Error(`scripts/lint/${rel} exports no run() — every guard file is a guard.`);
    }
    guards.push({ name: mod.name ?? rel, describe: mod.describe ?? '', run: mod.run });
  }
  return guards.sort((a, b) => a.name.localeCompare(b.name));
}

const guards = await loadGuards();
step(`Guards (${guards.length})`);

let failed = 0;
for (const guard of guards) {
  const findings = await guard.run();
  if (findings.length === 0) {
    ok(`${guard.name} — ${guard.describe}`);
    continue;
  }
  failed += findings.length;
  fail(`${guard.name} — ${findings.length} violation${findings.length === 1 ? '' : 's'}`);
  for (const f of findings) console.error(`   ${formatFinding(f)}`);
}

process.exit(failed === 0 ? 0 : 1);
