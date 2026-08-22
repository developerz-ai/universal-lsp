#!/usr/bin/env bun
/** The script catalog. CLAUDE.md points here rather than listing every command. */
const CATALOG: Array<[string, string]> = [
  ['bin/setup', 'prerequisites → configure build/dev → link compile_commands.json'],
  ['bin/dev build [preset]', 'build (default preset: dev)'],
  ['bin/dev test [regex]', 'run the suite, or the tests matching regex'],
  ['bin/dev run <target>', 'build + run one binary'],
  ['bin/dev doctor', 'one row per thing that can drift'],
  ['bin/check --files <paths>', 'tightest tier — format + guards on those files'],
  ['bin/check --changed', 'scoped tier — the affected subsystems only'],
  ['bin/check', 'full gate ≡ CI, sanitizers included. Once, before push'],
  ['bin/fmt', 'clang-format --write over tracked C/C++ sources'],
  ['bun run scripts/lint/index.ts', 'repo guards only'],
  ['bun test scripts/', "the guards' own tests + the change detector's"],
  ['bun run scripts/changed.ts', 'print the affected-subsystem JSON (CI reads the same function)'],
];

console.log('\nUniversal LSP — commands\n');
for (const [cmd, what] of CATALOG) console.log(`  ${cmd.padEnd(30)} ${what}`);
console.log('\nDocs: docs/README.md · Conventions: CLAUDE.md\n');
