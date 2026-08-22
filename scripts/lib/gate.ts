/**
 * THE GATE — one definition, two runners.
 *
 * `bin/check` runs these steps in order. CI runs the same argv strings, and
 * `scripts/ci-parity.test.ts` fails the build if a step exists in one place and
 * not the other. Drift here is the expensive kind: it teaches the agent that
 * pushing is how you find out, which is the slowest possible loop.
 *
 * Green locally ⇒ green CI. Anything else is a bug in this file.
 */
export type Scope = 'always' | 'build' | 'full';

export interface GateStep {
  id: string;
  label: string;
  argv: string[];
  /**
   * always — whole-tree, runs even on a docs-only change. A guard whose subject
   *          is the repo must not be gated on a subset of the repo.
   * build  — only when the change touches something that compiles.
   * full   — the slow lane: sanitizers. Skipped by `--fast`, never by CI.
   */
  scope: Scope;
}

export const GATE_STEPS: GateStep[] = [
  {
    id: 'format',
    label: 'clang-format',
    argv: ['bun', 'run', 'scripts/format.ts', '--check'],
    scope: 'always',
  },
  {
    id: 'ts-lint',
    label: 'biome (scripts/)',
    argv: ['bunx', 'biome', 'check', '.'],
    scope: 'always',
  },
  {
    id: 'guards',
    label: 'repo guards',
    argv: ['bun', 'run', 'scripts/lint/index.ts'],
    scope: 'always',
  },
  {
    id: 'script-tests',
    label: 'script + guard unit tests',
    argv: ['bun', 'test', 'scripts/'],
    scope: 'always',
  },
  {
    id: 'configure',
    label: 'cmake configure (dev)',
    argv: ['cmake', '--preset', 'dev'],
    scope: 'build',
  },
  {
    id: 'build',
    label: 'cmake build (dev)',
    argv: ['cmake', '--build', '--preset', 'dev'],
    scope: 'build',
  },
  {
    id: 'test',
    label: 'ctest (dev)',
    argv: ['ctest', '--preset', 'dev'],
    scope: 'build',
  },
  {
    id: 'standalone',
    label: 'graph-lite builds standalone',
    argv: [
      'cmake',
      '-S',
      'graph-lite',
      '-B',
      'build/graph-lite-standalone',
      '-G',
      'Ninja',
      '-DCMAKE_BUILD_TYPE=Debug',
    ],
    scope: 'build',
  },
  {
    id: 'standalone-build',
    label: 'graph-lite standalone build',
    argv: ['cmake', '--build', 'build/graph-lite-standalone'],
    scope: 'build',
  },
  {
    id: 'asan-configure',
    label: 'cmake configure (asan)',
    argv: ['cmake', '--preset', 'asan'],
    scope: 'full',
  },
  {
    id: 'asan-build',
    label: 'cmake build (asan)',
    argv: ['cmake', '--build', '--preset', 'asan'],
    scope: 'full',
  },
  {
    id: 'asan-test',
    label: 'ctest under ASan + UBSan',
    argv: ['ctest', '--preset', 'asan'],
    scope: 'full',
  },
];

export const stepCommand = (step: GateStep) => step.argv.join(' ');
