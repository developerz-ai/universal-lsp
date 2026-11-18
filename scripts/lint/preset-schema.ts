/**
 * A CMakePresets.json must be readable by CMake 3.24, the project's supported
 * floor, or declare and align a higher floor — otherwise `cmake --preset …`
 * dies on a supported toolchain with "Unsupported preset version".
 */
import { Glob } from 'bun';
import { REPO_ROOT } from '../lib/git.ts';
import { type Finding, lines } from '../lib/guard.ts';

const SCAN = '**/CMakePresets.json';
/** Build trees hold preset files fetched from third parties — not ours to fix. */
const SKIP = [/^build\//, /^node_modules\//, /^tmp\//, /^\.git\//];
const PROJECT_CMAKE_FLOOR = '3.24';

/** cmake-presets(7): the first CMake release that parses each schema version. */
const SCHEMA_SUPPORT: Record<number, string> = {
  1: '3.19',
  2: '3.20',
  3: '3.21',
  4: '3.23',
  5: '3.24',
  6: '3.25',
  7: '3.27',
  8: '3.28',
  9: '3.30',
  10: '3.31',
  11: '4.3',
  12: '4.4',
  13: '4.5',
};

interface PresetFile {
  version?: number;
  cmakeMinimumRequired?: { major?: number; minor?: number; patch?: number };
}

/** -1, 0 or 1 comparing dotted "3.24" style versions numerically. */
function compareVersions(a: string, b: string): number {
  const left = a.split('.').map(Number);
  const right = b.split('.').map(Number);
  for (let i = 0; i < Math.max(left.length, right.length); i += 1) {
    const diff = (left[i] ?? 0) - (right[i] ?? 0);
    if (diff !== 0) return diff < 0 ? -1 : 1;
  }
  return 0;
}

/** Highest schema version the given declared CMake minimum can parse, if any. */
function highestSchemaFor(declared: string): number | undefined {
  const candidates = Object.entries(SCHEMA_SUPPORT)
    .filter(([, cmake]) => compareVersions(cmake, declared) <= 0)
    .map(([version]) => Number(version));
  return candidates.length === 0 ? undefined : Math.max(...candidates);
}

export const name = 'preset-schema';
export const describe =
  'each CMakePresets.json schema version is parseable by the project’s supported CMake floor';

export async function run(root: string = REPO_ROOT): Promise<Finding[]> {
  const findings: Finding[] = [];

  for await (const rel of new Glob(SCAN).scan({ cwd: root })) {
    if (SKIP.some((re) => re.test(rel))) continue;
    const text = await lines(`${root}/${rel}`);
    const versionLine = text.findIndex((line) => /"version"\s*:/.test(line)) + 1;
    const preset = JSON.parse(text.join('\n')) as PresetFile;

    const version = preset.version;
    if (typeof version !== 'number') {
      findings.push({
        file: rel,
        line: versionLine || 1,
        message:
          'No numeric "version" — CMake refuses a preset file without one. Add the lowest schema ' +
          'that covers the fields used, e.g. "version": 5.',
      });
      continue;
    }

    const required = SCHEMA_SUPPORT[version];
    if (required === undefined) {
      findings.push({
        file: rel,
        line: versionLine,
        message:
          `Preset schema version ${version} is absent from the cmake-presets(7) support table, so its ` +
          `minimum CMake is unknown. Extend SCHEMA_SUPPORT in scripts/lint/preset-schema.ts with the ` +
          `first CMake release that parses it. Known versions: ${Object.keys(SCHEMA_SUPPORT).join(', ')}.`,
      });
      continue;
    }

    const minimum = preset.cmakeMinimumRequired;
    if (minimum === undefined) {
      if (compareVersions(PROJECT_CMAKE_FLOOR, required) >= 0) continue;
      findings.push({
        file: rel,
        line: versionLine,
        message:
          `Preset schema version ${version} needs CMake ${required} or newer, but this file omits ` +
          `cmakeMinimumRequired and the project supports CMake ${PROJECT_CMAKE_FLOOR}. Lower "version" ` +
          `to ${highestSchemaFor(PROJECT_CMAKE_FLOOR)}, or raise the CMake floor everywhere it is declared.`,
      });
      continue;
    }
    if (typeof minimum.major !== 'number' || typeof minimum.minor !== 'number') {
      findings.push({
        file: rel,
        line: versionLine,
        message:
          `Preset schema version ${version} has a "cmakeMinimumRequired" whose "major"/"minor" are ` +
          `not numbers, so the toolchain it promises cannot be read. Give both as integers, e.g. ` +
          `{"major": ${required.split('.')[0]}, "minor": ${required.split('.')[1]}, "patch": 0}.`,
      });
      continue;
    }

    const declared = `${minimum.major}.${minimum.minor}`;
    if (compareVersions(declared, required) >= 0) continue;

    const usable = highestSchemaFor(declared);
    findings.push({
      file: rel,
      line: versionLine,
      message:
        `Preset schema version ${version} needs CMake ${required} or newer, but this file declares ` +
        `cmakeMinimumRequired ${declared} — a ${declared} user gets "Unsupported preset version". ` +
        (usable === undefined
          ? `No known schema version supports CMake ${declared}; raise cmakeMinimumRequired to ${required}.`
          : `Lower "version" to ${usable} (see cmake-presets(7)), or raise cmakeMinimumRequired to ${required} ` +
            'and the CMake floor everywhere it is declared.'),
    });
  }

  return findings;
}
