/**
 * Dependencies flow one way: engine -> graph-lite, never back.
 *
 * Vocabulary leakage (the other guard) makes extraction expensive; a real
 * dependency makes it impossible — and it compiles fine, so nothing tells you.
 * This guard reads the include lines and the library's own CMakeLists, which is
 * where a `target_link_libraries(graph-lite PUBLIC ulsp::engine)` would hide.
 */
import { Glob } from 'bun';
import { REPO_ROOT } from '../lib/git.ts';
import { type Finding, lines } from '../lib/guard.ts';

const FORBIDDEN_INCLUDES = [
  /#\s*include\s*[<"](engine|analysis|daemon|cli)\//,
  /#\s*include\s*[<"]lua/,
  /#\s*include\s*[<"]tree_sitter/,
];

const FORBIDDEN_CMAKE = /\b(ulsp::|ulsp_engine|ulsp_analysis)\b/;

export const name = 'graph-lite-boundary';
export const describe = 'graph-lite depends on nothing in this repo';

export async function run(root: string = REPO_ROOT): Promise<Finding[]> {
  const findings: Finding[] = [];

  for await (const rel of new Glob('graph-lite/**/*.{h,hpp,cpp,c}').scan({ cwd: root })) {
    (await lines(`${root}/${rel}`)).forEach((text, i) => {
      for (const re of FORBIDDEN_INCLUDES) {
        if (!re.test(text)) continue;
        findings.push({
          file: rel,
          line: i + 1,
          message:
            `graph-lite includes a consumer header: "${text.trim()}". Dependencies flow one ` +
            `way (docs/spec.md §2) — invert it: pass the data in, or move the logic to ` +
            `analysis/. Guard: scripts/lint/graph-lite-boundary.ts`,
        });
      }
    });
  }

  const cmake = `${root}/graph-lite/CMakeLists.txt`;
  (await lines(cmake)).forEach((text, i) => {
    if (text.trimStart().startsWith('#')) return;
    if (!FORBIDDEN_CMAKE.test(text)) return;
    findings.push({
      file: 'graph-lite/CMakeLists.txt',
      line: i + 1,
      message:
        `graph-lite links a target from this repo: "${text.trim()}". It must configure and ` +
        `build standalone (\`cmake --preset graph-lite-standalone\`). ` +
        `Guard: scripts/lint/graph-lite-boundary.ts`,
    });
  });

  return findings;
}
