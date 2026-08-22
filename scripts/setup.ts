#!/usr/bin/env bun
/**
 * Fresh clone → configured build tree, zero manual steps. Idempotent: re-run it
 * whenever local state is confusing.
 */
import { has, run, runOrThrow } from './lib/cmd.ts';
import { REPO_ROOT } from './lib/git.ts';
import { fail, ok, step, warn } from './lib/log.ts';

interface Prereq {
  bin: string;
  why: string;
  install: string;
  required: boolean;
}

const PREREQS: Prereq[] = [
  { bin: 'cmake', why: 'build system (≥3.24)', install: 'apt install cmake', required: true },
  {
    bin: 'ninja',
    why: 'generator used by every preset',
    install: 'apt install ninja-build',
    required: true,
  },
  { bin: 'g++', why: 'C++20 compiler', install: 'apt install g++', required: true },
  {
    bin: 'git',
    why: 'dependency fetch + change detection',
    install: 'apt install git',
    required: true,
  },
  {
    bin: 'clang-format',
    why: 'the format gate',
    install: 'apt install clang-format',
    required: true,
  },
  {
    bin: 'clang-tidy',
    why: 'static analysis job (CI runs it regardless)',
    install: 'apt install clang-tidy',
    required: false,
  },
  { bin: 'ccache', why: 'faster rebuilds', install: 'apt install ccache', required: false },
];

step('Checking prerequisites');
let blocked = false;
for (const p of PREREQS) {
  if (await has(p.bin)) {
    const v = (await run([p.bin, '--version'])).stdout.split('\n')[0] ?? '';
    ok(`${p.bin.padEnd(13)} ${v.slice(0, 60)}`);
    continue;
  }
  if (p.required) {
    fail(`${p.bin.padEnd(13)} MISSING — ${p.why}. Install: ${p.install}`);
    blocked = true;
  } else {
    warn(`${p.bin.padEnd(13)} missing (optional) — ${p.why}. Install: ${p.install}`);
  }
}
if (blocked) process.exit(1);

step('Configuring the dev build tree (fetches GoogleTest on first run)');
await runOrThrow(['cmake', '--preset', 'dev'], REPO_ROOT);

step('Linking compile_commands.json for clangd');
await run(['ln', '-sf', 'build/dev/compile_commands.json', 'compile_commands.json'], REPO_ROOT);
ok('compile_commands.json → build/dev/');

ok('Setup complete. Next: bin/check   (full gate)   ·   bin/dev doctor   (environment)');
