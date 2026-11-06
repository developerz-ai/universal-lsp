#!/usr/bin/env bun
/**
 * Fresh clone → configured build tree, zero manual steps. Idempotent: re-run it
 * whenever local state is confusing.
 */
import { has, run, runOrThrow } from './lib/cmd.ts';
import { REPO_ROOT } from './lib/git.ts';
import { fail, ok, step, warn } from './lib/log.ts';
import { clangFormatMismatch, installMissing, missingRequired, prereqs } from './lib/prereqs.ts';

const CLANG_FORMAT = process.env.CLANG_FORMAT || 'clang-format';
const PREREQS = prereqs(CLANG_FORMAT);
const shell = { has, run, runOrThrow };

const installed = await installMissing(await missingRequired(PREREQS, shell), shell);
if (installed.length > 0) ok(`installed (apt, passwordless sudo): ${installed.join(' ')}`);

step('Checking prerequisites');
let blocked = false;
for (const p of PREREQS) {
  if (await has(p.bin)) {
    const out = (await run([p.bin, '--version'])).stdout;
    const mismatch = p.bin === CLANG_FORMAT ? clangFormatMismatch(out) : null;
    if (mismatch !== null) {
      fail(`${p.bin.padEnd(13)} ${mismatch}`);
      blocked = true;
      continue;
    }
    ok(`${p.bin.padEnd(13)} ${(out.split('\n')[0] ?? '').slice(0, 60)}`);
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
