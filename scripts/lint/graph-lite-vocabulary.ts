/**
 * graph-lite must never name a consumer's domain concept.
 *
 * WHY THIS IS MECHANICAL: the library's whole value proposition is that it is
 * generic — a property graph anyone can embed — and its first consumer is a
 * large, opinionated system that will happily push its vocabulary downward one
 * innocuous identifier at a time. By the time someone notices, extraction is a
 * rewrite instead of a `git mv`. Review discipline does not catch this; a
 * regex does.
 *
 * SCANNED — everything that ships when the library is extracted:
 *   graph-lite/include/**, graph-lite/src/**, graph-lite/README.md,
 *   tests/graph-lite/**, docs/graph-lite/**
 *
 * NOT SCANNED — repo-side glue that stays behind, and says so:
 *   graph-lite/CMakeLists.txt, graph-lite/CLAUDE.md
 *
 * Fix: rename the identifier to its generic equivalent (a "symbol" is a node
 * type the CONSUMER defines; the translation belongs in analysis/).
 *
 * FENCED CODE BLOCKS IN MARKDOWN ARE SKIPPED, and only there. A doc example
 * calling a THIRD-PARTY API cannot be renamed — Bun's FFI really does return a
 * `symbols` object — and a guard that fires on someone else's API name is a guard
 * that gets disabled. Source files are scanned in full, fences or not, because
 * `SymbolNode` in a header is the leak this exists to catch.
 */
import { Glob } from 'bun';
import { REPO_ROOT } from '../lib/git.ts';
import { type Finding, lines } from '../lib/guard.ts';
import { ALLOWLIST } from './graph-lite-vocabulary-allowlist.ts';

const SCAN = [
  'graph-lite/include/**/*.{h,hpp}',
  'graph-lite/src/**/*.{cpp,h,hpp}',
  'graph-lite/README.md',
  'tests/graph-lite/**/*.{cpp,h}',
  'docs/graph-lite/**/*.md',
];

/**
 * Banned single words, matched per IDENTIFIER WORD, not per regex word boundary.
 * `\bsymbol\b` does not match `SymbolNode` — and `SymbolNode` is exactly the shape
 * the leak takes, so the guard tokenises camelCase and snake_case first.
 *
 * Generic C++ words (scope, reference, node, file) are deliberately absent: a
 * guard with false positives gets disabled, and a disabled guard is worse than
 * none.
 */
const BANNED_WORDS: Record<string, string> = {
  lsp: 'protocol vocabulary',
  symbol: 'consumer concept — a node type the consumer defines',
  hover: 'consumer concept',
  completion: 'consumer concept',
  diagnostic: 'consumer concept',
  document: 'consumer concept — call it a node',
  workspace: 'consumer concept',
  plugin: 'consumer concept',
  lua: 'consumer dependency',
  treesitter: 'consumer dependency',
};

/** Multi-word forms, matched against the line with separators normalised. */
const BANNED_PHRASES: Array<[RegExp, string]> = [
  [/\blanguage servers?\b/, 'protocol vocabulary'],
  [/\btree sitter\b/, 'consumer dependency'],
  [/\bcall graphs?\b/, 'consumer concept — it is a typed edge set'],
  [/\bimport graphs?\b/, 'consumer concept — it is a typed edge set'],
  [/\bgo ?to definition\b/, 'consumer concept'],
  [/\buniversal lsp\b/, 'names the consumer'],
];

/** "SymbolNode" -> ["symbol", "node"]; "symbol_node" -> ["symbol", "node"]. */
export function identifierWords(line: string): string[] {
  return line
    .split(/[^A-Za-z0-9]+/)
    .flatMap((chunk) => chunk.replace(/([a-z0-9])([A-Z])/g, '$1 $2').split(' '))
    .filter(Boolean)
    .map((w) => w.toLowerCase());
}

const singular = (word: string) => (word.endsWith('s') ? word.slice(0, -1) : word);

function violations(line: string): Array<[string, string]> {
  const hits: Array<[string, string]> = [];

  for (const word of identifierWords(line)) {
    const why = BANNED_WORDS[word] ?? BANNED_WORDS[singular(word)];
    if (why) hits.push([word, why]);
  }

  const normalised = line.toLowerCase().replace(/[^a-z0-9]+/g, ' ');
  for (const [re, why] of BANNED_PHRASES) {
    const hit = normalised.match(re);
    if (hit) hits.push([hit[0], why]);
  }

  return hits;
}

export const name = 'graph-lite-vocabulary';
export const describe = 'graph-lite names no consumer-domain concept';

export async function run(root: string = REPO_ROOT): Promise<Finding[]> {
  const findings: Finding[] = [];

  for (const pattern of SCAN) {
    for await (const rel of new Glob(pattern).scan({ cwd: root })) {
      if (ALLOWLIST.includes(rel)) continue;
      const content = await lines(`${root}/${rel}`);
      const markdown = rel.endsWith('.md');
      let inFence = false;

      content.forEach((text, i) => {
        if (markdown && text.trimStart().startsWith('```')) {
          inFence = !inFence;
          return;
        }
        if (markdown && inFence) return;

        for (const [hit, why] of violations(text)) {
          findings.push({
            file: rel,
            line: i + 1,
            message:
              `"${hit}" is ${why}. graph-lite is generic by contract (docs/spec.md §2) — ` +
              `the translation from that concept to a typed node/edge belongs in analysis/. ` +
              `Guard: scripts/lint/graph-lite-vocabulary.ts`,
          });
        }
      });
    }
  }

  return findings;
}
