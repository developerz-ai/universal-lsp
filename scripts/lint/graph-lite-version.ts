/**
 * graph-lite's version is stated by the header macros; everything else restates it.
 *
 * WHY THIS IS MECHANICAL: the library is versioned independently (docs/spec.md
 * §2) and released by tag, but "the version" exists only as separate
 * declarations with nothing tying them together. No pair of them is checked at
 * build time: the startup check (tests/graph-lite/version_test.cpp) compares
 * the macros to the BINARY, not to the other statements, and a header/CMake
 * mismatch configures clean. Statements hardcoded to one version have already
 * shipped next to a differently-named release tag because only a human was
 * looking.
 *
 * THE CANONICAL FORM: GRAPH_LITE_VERSION_MAJOR/_MINOR/_PATCH in graph-lite.h
 * are THE version — part of the published contract every binding pins, and
 * what the startup check compares the linked binary against. The restatements
 * must equal it:
 *   graph-lite/include/graph-lite.h   GRAPH_LITE_VERSION_STRING
 *   graph-lite/CMakeLists.txt         project(... VERSION x.y.z ...) and
 *                                     set(GRAPH_LITE_VERSION x.y.z)
 * When a restatement disagrees, THE RESTATEMENT MOVES, never the macros —
 * which makes every finding a concrete "set this line to X".
 *
 * NOT PARSED — prose that mentions versions on purpose (README.md,
 * docs/graph-lite/**, CHANGELOG.md). A changelog MUST list superseded versions
 * and a release note is written after the fact; a text guard over those is a
 * false-positive machine, and this one does the real work.
 */
import { REPO_ROOT } from '../lib/git.ts';
import { type Finding, lines } from '../lib/guard.ts';

const HEADER = 'graph-lite/include/graph-lite.h';
const CMAKE = 'graph-lite/CMakeLists.txt';
const GUARD = 'scripts/lint/graph-lite-version.ts';

type Component = 'major' | 'minor' | 'patch';
type Version = Record<Component, string>;

const COMPONENT_ORDER: Component[] = ['major', 'minor', 'patch'];
const COMPONENTS: Record<string, Component> = {
  MAJOR: 'major',
  MINOR: 'minor',
  PATCH: 'patch',
};

/** A CMake line that restates the version. version is null when unreadable. */
interface Restatement {
  name: string;
  line: number;
  version: Version | null;
}

const TRIPLE = /^(\d+)\.(\d+)\.(\d+)$/;
const HEADER_MACRO = /^#define\s+GRAPH_LITE_VERSION_(MAJOR|MINOR|PATCH)\s+(\d+)\s*$/;
const HEADER_STRING = /^#define\s+GRAPH_LITE_VERSION_STRING\s+"(\d+\.\d+\.\d+)"\s*$/;
const HEADER_KNOWN = /^#define\s+GRAPH_LITE_VERSION_(?:MAJOR|MINOR|PATCH|STRING)\b/;
const CMAKE_PROJECT = /\bproject\s*\(([^)]*)\)/;
const CMAKE_SET = /^\s*set\s*\(\s*GRAPH_LITE_VERSION\s+(\S+)\s*\)\s*$/;
const CMAKE_SET_KNOWN = /^\s*set\s*\(\s*GRAPH_LITE_VERSION\b/;

/** C block-comment interiors (and openers) are never version statements. */
const isHeaderComment = (text: string) => /^\s*(\*|\/\/|\/\*)/.test(text);
/** CMake comments are never version statements. */
const isCmakeComment = (text: string) => /^\s*#/.test(text);

const stated = (v: Version) => `${v.major}.${v.minor}.${v.patch}`;

function asTriple(text: string): Version {
  const m = TRIPLE.exec(text);
  if (!m) throw new Error(`not an x.y.z triple: "${text}"`);
  return { major: m[1], minor: m[2], patch: m[3] };
}

function diffComponents(a: Version, b: Version): Component[] {
  return COMPONENT_ORDER.filter((c) => a[c] !== b[c]);
}

function missing(file: string, what: string): Finding {
  return {
    file,
    line: 1,
    message:
      `${file} states no ${what} — a version statement is missing from the ` +
      `lockstep (or the file itself is gone). Guard: ${GUARD}`,
  };
}

interface HeaderParse {
  findings: Finding[];
  /** The canonical version — only when all three macros state one. */
  version: Version | null;
  /** What the STRING macro states, when it states something readable. */
  str: { line: number; version: Version } | null;
}

async function parseHeader(root: string): Promise<HeaderParse> {
  const out: HeaderParse = { findings: [], version: null, str: null };
  let content: string[];
  try {
    content = await lines(`${root}/${HEADER}`);
  } catch {
    out.findings.push(missing(HEADER, 'GRAPH_LITE_VERSION_* macros'));
    return out;
  }

  const macros: Partial<Record<Component, string>> = {};
  content.forEach((text, i) => {
    const line = i + 1;
    if (isHeaderComment(text)) return;

    const macro = HEADER_MACRO.exec(text);
    if (macro) {
      macros[COMPONENTS[macro[1]]] = macro[2];
      return;
    }
    const str = HEADER_STRING.exec(text);
    if (str) {
      out.str = { line, version: asTriple(str[1]) };
      return;
    }
    if (HEADER_KNOWN.test(text)) {
      out.findings.push({
        file: HEADER,
        line,
        message:
          `"${text.trim()}" is not a readable version statement — a ` +
          `GRAPH_LITE_VERSION_* macro needs a literal number and _STRING a ` +
          `quoted "x.y.z", so this guard can compare them. Guard: ${GUARD}`,
      });
    }
  });

  const absent = COMPONENT_ORDER.filter((c) => macros[c] === undefined);
  for (const c of absent) {
    out.findings.push(missing(HEADER, `GRAPH_LITE_VERSION_${c.toUpperCase()} macro`));
  }
  if (absent.length === 0) {
    out.version = {
      major: macros.major ?? '',
      minor: macros.minor ?? '',
      patch: macros.patch ?? '',
    };
  }
  return out;
}

interface CmakeParse {
  findings: Finding[];
  restatements: Restatement[];
}

async function parseCmake(root: string): Promise<CmakeParse> {
  const out: CmakeParse = { findings: [], restatements: [] };
  let content: string[];
  try {
    content = await lines(`${root}/${CMAKE}`);
  } catch {
    out.findings.push(missing(CMAKE, 'project(... VERSION ...) / set(GRAPH_LITE_VERSION)'));
    return out;
  }

  let sawProject = false;
  let sawSet = false;
  content.forEach((text, i) => {
    const line = i + 1;
    if (isCmakeComment(text)) return;

    const project = CMAKE_PROJECT.exec(text);
    if (project) {
      sawProject = true;
      const statedVersion = /\bVERSION\s+(\S+)/.exec(project[1]);
      if (statedVersion && TRIPLE.test(statedVersion[1])) {
        out.restatements.push({
          name: 'project(... VERSION ...)',
          line,
          version: asTriple(statedVersion[1]),
        });
      } else {
        out.findings.push({
          file: CMAKE,
          line,
          message:
            `"${text.trim()}" carries no literal x.y.z VERSION — project() ` +
            `must state graph-lite's version directly so this guard can ` +
            `compare it. Guard: ${GUARD}`,
        });
      }
    }

    if (CMAKE_SET_KNOWN.test(text)) {
      sawSet = true;
      const set = CMAKE_SET.exec(text);
      if (set && TRIPLE.test(set[1])) {
        out.restatements.push({
          name: 'set(GRAPH_LITE_VERSION)',
          line,
          version: asTriple(set[1]),
        });
      } else {
        out.findings.push({
          file: CMAKE,
          line,
          message:
            `"${text.trim()}" does not set a literal x.y.z — ` +
            `set(GRAPH_LITE_VERSION) must state the version directly, not ` +
            `through a variable this guard cannot resolve. Guard: ${GUARD}`,
        });
      }
    }
  });

  if (!sawProject) out.findings.push(missing(CMAKE, 'project(... VERSION x.y.z ...)'));
  if (!sawSet) out.findings.push(missing(CMAKE, 'set(GRAPH_LITE_VERSION x.y.z)'));
  return out;
}

export const name = 'graph-lite-version';
export const describe = "graph-lite's restated versions match the header macros";

export async function run(root: string = REPO_ROOT): Promise<Finding[]> {
  const header = await parseHeader(root);
  const cmake = await parseCmake(root);
  const findings: Finding[] = [...header.findings, ...cmake.findings];
  const canonical = header.version;
  if (!canonical) return findings;

  if (header.str && stated(header.str.version) !== stated(canonical)) {
    const diff = diffComponents(header.str.version, canonical);
    findings.push({
      file: HEADER,
      line: header.str.line,
      message:
        `GRAPH_LITE_VERSION_STRING states "${stated(header.str.version)}" but the ` +
        `GRAPH_LITE_VERSION_MAJOR/MINOR/PATCH macros state ${stated(canonical)} ` +
        `(${diff.join(', ')}) — the string restates the macros: set it to ` +
        `"${stated(canonical)}". Guard: ${GUARD}`,
    });
  }

  for (const r of cmake.restatements) {
    if (!r.version || stated(r.version) === stated(canonical)) continue;
    const diff = diffComponents(r.version, canonical);
    findings.push({
      file: CMAKE,
      line: r.line,
      message:
        `${r.name} states ${stated(r.version)} but graph-lite.h's version macros ` +
        `state ${stated(canonical)} (${diff.join(', ')}) — the header is the ` +
        `published contract every binding pins, so this line moves: set it to ` +
        `${stated(canonical)}. Guard: ${GUARD}`,
    });
  }

  return findings;
}
