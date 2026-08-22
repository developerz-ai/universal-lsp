#!/usr/bin/env bun
/**
 * One truthful row per thing that can drift. "Present" means a real probe
 * answered, never that a file happens to exist — half of "the agent is broken"
 * is "the box is wrong", and a doctor that lies costs more than no doctor.
 */
import { existsSync } from 'node:fs';

import { has, run } from './lib/cmd.ts';
import { REPO_ROOT } from './lib/git.ts';
import { fail, ok, warn } from './lib/log.ts';

interface Row {
  name: string;
  probe: () => Promise<{ good: boolean; detail: string; optional?: boolean }>;
}

const version = async (bin: string) => (await run([bin, '--version'])).stdout.split('\n')[0] ?? '';

const ROWS: Row[] = [
  {
    name: 'cmake',
    probe: async () => {
      if (!(await has('cmake'))) return { good: false, detail: 'missing — run bin/setup' };
      const v = (await version('cmake')).match(/(\d+)\.(\d+)/);
      const major = Number(v?.[1] ?? 0);
      const minor = Number(v?.[2] ?? 0);
      const good = major > 3 || (major === 3 && minor >= 24);
      return {
        good,
        detail: good ? `${major}.${minor}` : `${major}.${minor} — presets need ≥ 3.24`,
      };
    },
  },
  {
    name: 'compiler',
    probe: async () => {
      const cxx = (await has('g++')) ? 'g++' : (await has('clang++')) ? 'clang++' : '';
      if (!cxx) return { good: false, detail: 'no C++ compiler on PATH' };
      return { good: true, detail: await version(cxx) };
    },
  },
  {
    name: 'clang-format',
    probe: async () =>
      (await has('clang-format'))
        ? { good: true, detail: await version('clang-format') }
        : { good: false, detail: 'missing — the format gate cannot run' },
  },
  {
    name: 'clang-tidy',
    probe: async () =>
      (await has('clang-tidy'))
        ? { good: true, detail: await version('clang-tidy') }
        : {
            good: false,
            optional: true,
            detail: 'missing — CI still runs it; local static analysis is off',
          },
  },
  {
    name: 'build tree',
    probe: async () =>
      existsSync(`${REPO_ROOT}/build/dev/CMakeCache.txt`)
        ? { good: true, detail: 'build/dev configured' }
        : { good: false, detail: 'not configured — run bin/setup' },
  },
  {
    name: 'compile_commands',
    probe: async () =>
      existsSync(`${REPO_ROOT}/compile_commands.json`)
        ? { good: true, detail: 'symlinked for clangd' }
        : {
            good: false,
            optional: true,
            detail: 'missing — clangd will not resolve includes. bin/setup fixes it',
          },
  },
  {
    name: 'tests registered',
    probe: async () => {
      const res = await run(['ctest', '--preset', 'dev', '-N'], REPO_ROOT);
      const count = (res.stdout.match(/^\s*Test\s+#\d+/gm) ?? []).length;
      return count > 0
        ? { good: true, detail: `${count} test(s) discovered` }
        : { good: false, detail: 'ctest discovered 0 tests — build first: bin/dev build' };
    },
  },
];

let bad = 0;
for (const row of ROWS) {
  const { good, detail, optional } = await row.probe();
  if (good) ok(`${row.name.padEnd(18)} ${detail}`);
  else if (optional) warn(`${row.name.padEnd(18)} ${detail}`);
  else {
    fail(`${row.name.padEnd(18)} ${detail}`);
    bad++;
  }
}

process.exit(bad === 0 ? 0 : 1);
