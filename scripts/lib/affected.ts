/**
 * ONE change detector, three consumers: `bin/check --changed` locally, the CI
 * `detect` job, and `bin/dev build`. A pure function of the changed-file list —
 * no git, no fs, no process — so it is trivially testable and cannot disagree
 * with itself across those three callers. If local scoping and CI scoping drift,
 * "green locally" stops meaning anything. Guard: scripts/ci-parity.test.ts
 */

export type Subsystem = 'graph-lite' | 'engine' | 'analysis' | 'daemon' | 'cli' | 'plugins';

/** Who must be re-verified when a subsystem changes. Dependencies flow one way. */
const DEPENDENTS: Record<Subsystem, Subsystem[]> = {
  'graph-lite': ['engine', 'analysis', 'daemon', 'cli'],
  engine: ['analysis', 'daemon', 'cli'],
  analysis: ['daemon', 'cli'],
  daemon: [],
  cli: [],
  plugins: [],
};

/** A change to any of these re-verifies everything — the build itself moved. */
const FULL_GATE_PATHS = [
  'CMakeLists.txt',
  'CMakePresets.json',
  'cmake/',
  'bin/',
  'scripts/',
  '.clang-format',
  '.clang-tidy',
  '.github/workflows/',
];

const OWNERS: Array<[string, Subsystem]> = [
  ['graph-lite/', 'graph-lite'],
  ['tests/graph-lite/', 'graph-lite'],
  ['engine/', 'engine'],
  ['tests/engine/', 'engine'],
  ['analysis/', 'analysis'],
  ['daemon/', 'daemon'],
  ['cli/', 'cli'],
  ['plugins/', 'plugins'],
  ['tests/plugins/', 'plugins'],
];

export interface Affected {
  /** Subsystems to build + test, dependents included. */
  subsystems: Subsystem[];
  /** A root/build-config change: verify everything. */
  fullGate: boolean;
  /** False for a docs-only or prose-only change. Guards still run regardless. */
  buildNeeded: boolean;
  /** CTest `-R` regex selecting the affected suites, or '' when nothing is selected. */
  ctestFilter: string;
}

const CTEST_PREFIX: Record<Subsystem, string> = {
  'graph-lite': 'graph_lite_',
  engine: 'engine_',
  analysis: 'analysis_',
  daemon: 'daemon_',
  cli: 'cli_',
  plugins: 'plugins_',
};

const ALL = Object.keys(DEPENDENTS) as Subsystem[];

export function computeAffected(changedFiles: string[]): Affected {
  const fullGate = changedFiles.some((f) => FULL_GATE_PATHS.some((p) => f.startsWith(p)));

  const direct = new Set<Subsystem>();
  for (const file of changedFiles) {
    const owner = OWNERS.find(([prefix]) => file.startsWith(prefix));
    if (owner) direct.add(owner[1]);
  }

  const selected = new Set<Subsystem>(fullGate ? ALL : direct);
  if (!fullGate) for (const s of direct) for (const dep of DEPENDENTS[s]) selected.add(dep);

  const subsystems = ALL.filter((s) => selected.has(s));
  return {
    subsystems,
    fullGate,
    buildNeeded: subsystems.length > 0,
    ctestFilter: subsystems.length ? `^(${subsystems.map((s) => CTEST_PREFIX[s]).join('|')})` : '',
  };
}
