#!/usr/bin/env bun
/**
 * clang-format over every C/C++ source in the tree. `--check` fails without
 * writing (the gate + CI mode); bare invocation rewrites in place (`bin/fmt`).
 *
 * The agent never makes a formatting decision — .clang-format does.
 *
 * FILES COME FROM THE FILESYSTEM, NOT `git ls-files`. The first version asked
 * git, and in a tree where nothing was staged yet it found zero files, printed
 * "no C/C++ sources tracked yet", and exited 0 — a gate that answers with
 * silence instead of an error. CI caught the unformatted code on the very first
 * push, over a local run that had just reported green.
 *
 * THE VERSION IS PINNED. clang-format's output changes between major releases,
 * so an unpinned formatter means CI and a developer disagree about what
 * "formatted" means — which is the same class of bug as the local gate and CI
 * running different commands.
 */
import { Glob } from 'bun';

import { has, run, runOrThrow } from './lib/cmd.ts';
import { REPO_ROOT } from './lib/git.ts';
import { fail, ok, step } from './lib/log.ts';

const REQUIRED_MAJOR = 18;
const BIN = process.env.CLANG_FORMAT || 'clang-format';

const SOURCES = '{graph-lite,engine,analysis,daemon,cli,tests,plugins}/**/*.{cpp,h,hpp,c,cc}';

const check = process.argv.includes('--check');

if (!(await has(BIN))) {
  fail(`${BIN} not found — run bin/setup, or: sudo apt install clang-format-${REQUIRED_MAJOR}`);
  process.exit(127);
}

const version = (await run([BIN, '--version'])).stdout;
const major = Number(version.match(/version (\d+)/)?.[1] ?? 0);
if (major !== REQUIRED_MAJOR) {
  fail(
    `${BIN} is version ${major}, this repo formats with ${REQUIRED_MAJOR}. ` +
      `Output differs between majors, so a mismatch means your "formatted" and CI's ` +
      `disagree. Install clang-format-${REQUIRED_MAJOR} and re-run, or set ` +
      `CLANG_FORMAT=clang-format-${REQUIRED_MAJOR}.`,
  );
  process.exit(1);
}

const files: string[] = [];
for await (const rel of new Glob(SOURCES).scan({ cwd: REPO_ROOT })) files.push(rel);
files.sort();

if (files.length === 0) {
  // Not a pass. Every subsystem directory is committed, so zero matches means the
  // glob is wrong or the checkout is broken — either way the gate learned nothing.
  fail(`no C/C++ sources matched ${SOURCES} — the check verified nothing. Fix the glob.`);
  process.exit(1);
}

step(`${BIN} ${check ? '--check' : '--write'} (${files.length} files)`);

if (!check) {
  await runOrThrow([BIN, '-i', ...files], REPO_ROOT);
  ok('formatted');
  process.exit(0);
}

const res = await run([BIN, '--dry-run', '--Werror', ...files], REPO_ROOT);
if (res.code === 0) {
  ok(`${BIN} clean`);
  process.exit(0);
}

fail('clang-format found unformatted code. Fix: bin/fmt');
console.error(res.stderr.trim());
process.exit(1);
