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
  /** The Ubuntu package that provides `bin`. */
  apt: string;
  required: boolean;
}

const PREREQS: Prereq[] = [
  { bin: 'cmake', why: 'build system (≥3.24)', apt: 'cmake', required: true },
  { bin: 'ninja', why: 'generator used by every preset', apt: 'ninja-build', required: true },
  { bin: 'g++', why: 'C++20 compiler', apt: 'g++', required: true },
  { bin: 'git', why: 'dependency fetch + change detection', apt: 'git', required: true },
  { bin: 'clang-format', why: 'the format gate', apt: 'clang-format', required: true },
  {
    bin: 'clang-tidy',
    why: 'static analysis job (CI runs it regardless)',
    apt: 'clang-tidy',
    required: false,
  },
  { bin: 'ccache', why: 'faster rebuilds', apt: 'ccache', required: false },
];

async function missingRequired(): Promise<Prereq[]> {
  const missing: Prereq[] = [];
  for (const p of PREREQS) if (p.required && !(await has(p.bin))) missing.push(p);
  return missing;
}

/**
 * A box with apt and passwordless sudo (a developerz.ai fleet VM, CI) gets the
 * missing REQUIRED packages installed — the same packages CI's workflows install.
 * Anywhere else nothing is installed and the report below names the command.
 * Ubuntu 24.04's `clang-format` is 18, the major scripts/format.ts pins.
 */
async function installMissing(missing: Prereq[]): Promise<void> {
  if (missing.length === 0 || !(await has('apt-get')) || !(await has('sudo'))) return;
  if ((await run(['sudo', '-n', 'true'])).code !== 0) return;
  const pkgs = missing.map((p) => p.apt);
  step(`Installing ${pkgs.join(' ')} (apt, passwordless sudo)`);
  await runOrThrow(['sudo', '-n', 'apt-get', 'update', '-qq']);
  await runOrThrow([
    'sudo',
    '-n',
    'env',
    'DEBIAN_FRONTEND=noninteractive',
    'apt-get',
    'install',
    '-y',
    '-qq',
    ...pkgs,
  ]);
}

await installMissing(await missingRequired());

step('Checking prerequisites');
let blocked = false;
for (const p of PREREQS) {
  if (await has(p.bin)) {
    const v = (await run([p.bin, '--version'])).stdout.split('\n')[0] ?? '';
    ok(`${p.bin.padEnd(13)} ${v.slice(0, 60)}`);
    continue;
  }
  if (p.required) {
    fail(`${p.bin.padEnd(13)} MISSING — ${p.why}. Install: sudo apt install ${p.apt}`);
    blocked = true;
  } else {
    warn(`${p.bin.padEnd(13)} missing (optional) — ${p.why}. Install: sudo apt install ${p.apt}`);
  }
}
if (blocked) process.exit(1);

step('Configuring the dev build tree (fetches GoogleTest on first run)');
await runOrThrow(['cmake', '--preset', 'dev'], REPO_ROOT);

step('Linking compile_commands.json for clangd');
await run(['ln', '-sf', 'build/dev/compile_commands.json', 'compile_commands.json'], REPO_ROOT);
ok('compile_commands.json → build/dev/');

ok('Setup complete. Next: bin/check   (full gate)   ·   bin/dev doctor   (environment)');
