#!/usr/bin/env bun
/**
 * clang-format over every tracked C/C++ file. `--check` fails without writing
 * (the CI + gate mode); bare invocation rewrites in place (`bin/fmt`).
 *
 * The agent never makes a formatting decision — .clang-format does.
 */
import { has, run, runOrThrow } from './lib/cmd.ts';
import { REPO_ROOT, trackedFiles } from './lib/git.ts';
import { fail, ok, step } from './lib/log.ts';

const check = process.argv.includes('--check');

if (!(await has('clang-format'))) {
  fail('clang-format not found — run bin/setup, or apt install clang-format');
  process.exit(127);
}

const files = await trackedFiles(['*.cpp', '*.h', '*.hpp', '*.c', '*.cc']);

if (files.length === 0) {
  ok('clang-format — no C/C++ sources tracked yet');
  process.exit(0);
}

step(`clang-format ${check ? '--check' : '--write'} (${files.length} files)`);

if (!check) {
  await runOrThrow(['clang-format', '-i', ...files], REPO_ROOT);
  ok('formatted');
  process.exit(0);
}

const res = await run(['clang-format', '--dry-run', '--Werror', ...files], REPO_ROOT);
if (res.code === 0) {
  ok('clang-format clean');
  process.exit(0);
}

fail('clang-format found unformatted code. Fix: bin/fmt');
console.error(res.stderr.trim());
process.exit(1);
