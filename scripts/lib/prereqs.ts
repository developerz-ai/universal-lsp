/**
 * bin/setup's prerequisite logic, apart from the script so it can be tested:
 * which tools a box needs, when setup may install them, and whether the
 * clang-format it found is the major the format gate pins.
 */
import type { RunResult } from './cmd.ts';

/** The major scripts/format.ts formats with. Its output differs between majors. */
export const CLANG_FORMAT_MAJOR = 18;

export interface Prereq {
  bin: string;
  why: string;
  /** The Ubuntu package that provides `bin`. */
  apt: string;
  required: boolean;
}

/**
 * What setup checks. `clangFormat` is the binary the format gate calls — the
 * same `CLANG_FORMAT || 'clang-format'` scripts/format.ts resolves.
 */
export function prereqs(clangFormat = 'clang-format'): Prereq[] {
  return [
    { bin: 'cmake', why: 'build system (≥3.24)', apt: 'cmake', required: true },
    { bin: 'ninja', why: 'generator used by every preset', apt: 'ninja-build', required: true },
    { bin: 'g++', why: 'C++20 compiler', apt: 'g++', required: true },
    { bin: 'git', why: 'dependency fetch + change detection', apt: 'git', required: true },
    // The bare `clang-format` package, not `clang-format-18`: only it provides the
    // bare binary the gate calls by default. Its major is checked after install
    // (`clangFormatMismatch`).
    {
      bin: clangFormat,
      why: 'the format gate',
      apt: clangFormat === 'clang-format' ? 'clang-format' : `clang-format-${CLANG_FORMAT_MAJOR}`,
      required: true,
    },
    {
      bin: 'clang-tidy',
      why: 'static analysis job (CI runs it regardless)',
      apt: 'clang-tidy',
      required: false,
    },
    { bin: 'ccache', why: 'faster rebuilds', apt: 'ccache', required: false },
  ];
}

/** The subprocess calls the install decision makes (`./cmd.ts` in prod). */
export interface Shell {
  has(bin: string): Promise<boolean>;
  run(argv: string[]): Promise<RunResult>;
  runOrThrow(argv: string[]): Promise<void>;
}

export async function missingRequired(prereqs: Prereq[], sh: Shell): Promise<Prereq[]> {
  const missing: Prereq[] = [];
  for (const p of prereqs) if (p.required && !(await sh.has(p.bin))) missing.push(p);
  return missing;
}

/**
 * A box with apt and passwordless sudo (a developerz.ai fleet VM, CI) gets the
 * missing REQUIRED packages installed — the same packages CI's workflows install.
 * Anywhere else nothing is installed and setup's report names the command.
 * Returns the packages it installed, `[]` when it installed nothing.
 */
export async function installMissing(missing: Prereq[], sh: Shell): Promise<string[]> {
  if (missing.length === 0 || !(await sh.has('apt-get')) || !(await sh.has('sudo'))) return [];
  if ((await sh.run(['sudo', '-n', 'true'])).code !== 0) return [];
  const pkgs = missing.map((p) => p.apt);
  await sh.runOrThrow(['sudo', '-n', 'apt-get', 'update', '-qq']);
  await sh.runOrThrow([
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
  return pkgs;
}

/**
 * `null` when `clang-format --version` output is the pinned major; otherwise the
 * fix. An older Ubuntu's `clang-format` package (22.04 ships 14) passes the
 * presence check but the format gate refuses it.
 */
export function clangFormatMismatch(versionOutput: string): string | null {
  const major = Number(versionOutput.match(/version (\d+)/)?.[1] ?? 0);
  if (major === CLANG_FORMAT_MAJOR) return null;
  return (
    `is version ${major}, the format gate pins ${CLANG_FORMAT_MAJOR}. ` +
    `Install: sudo apt install clang-format-${CLANG_FORMAT_MAJOR}, ` +
    `then set CLANG_FORMAT=clang-format-${CLANG_FORMAT_MAJOR}`
  );
}
