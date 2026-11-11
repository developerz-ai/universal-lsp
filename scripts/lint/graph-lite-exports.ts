/**
 * The shared library hides implementation symbols; this guard keeps its explicit
 * C ABI export annotations limited to the declarations embedders may bind.
 */
import { REPO_ROOT } from '../lib/git.ts';
import { type Finding, lines } from '../lib/guard.ts';

const HEADER = 'graph-lite/include/graph-lite.h';

/** Every public C ABI entry point. Adding one is an audited ABI promise. */
const EXPORTED_FUNCTIONS = new Set([
  'graph_lite_version',
  'graph_lite_check_version',
  'graph_lite_version_string',
  'graph_lite_status_string',
  'graph_lite_graph_create',
  'graph_lite_graph_destroy',
  'graph_lite_add_node',
  'graph_lite_remove_node',
  'graph_lite_node_tag',
  'graph_lite_add_edge',
  'graph_lite_remove_edge',
  'graph_lite_edge_info',
  'graph_lite_node_count',
  'graph_lite_edge_count',
  'graph_lite_out_edges',
  'graph_lite_in_edges',
  'graph_lite_set_int',
  'graph_lite_set_double',
  'graph_lite_set_bool',
  'graph_lite_set_string',
  'graph_lite_property_kind',
  'graph_lite_get_int',
  'graph_lite_get_double',
  'graph_lite_get_bool',
  'graph_lite_get_string',
  'graph_lite_remove_property',
  'graph_lite_node_out_degree',
  'graph_lite_node_in_degree',
  'graph_lite_node_out_neighbors',
  'graph_lite_node_in_neighbors',
  'graph_lite_graph_memory_usage',
  'graph_lite_cursor_open',
  'graph_lite_cursor_next',
  'graph_lite_cursor_drain',
  'graph_lite_cursor_destroy',
  'graph_lite_traversal_open',
  'graph_lite_traversal_next',
  'graph_lite_traversal_partial',
  'graph_lite_traversal_destroy',
  'graph_lite_any_path',
  'graph_lite_shortest_path',
]);

const EXPORTED_DECLARATION = /\bGRAPH_LITE_API\s+[\s\S]*?\b(graph_lite_[a-z0-9_]+)\s*\(/g;

export const name = 'graph-lite-exports';
export const describe = 'graph-lite exports only its audited C ABI';

export async function run(root: string = REPO_ROOT): Promise<Finding[]> {
  const header = await lines(`${root}/${HEADER}`);
  const source = header.join('\n');
  const findings: Finding[] = [];

  for (const match of source.matchAll(EXPORTED_DECLARATION)) {
    const functionName = match[1];
    if (!functionName || EXPORTED_FUNCTIONS.has(functionName)) continue;

    findings.push({
      file: HEADER,
      line: source.slice(0, match.index).split('\n').length,
      message:
        `"${functionName}" is marked GRAPH_LITE_API but is absent from the audited C ABI ` +
        `allowlist. Remove the export annotation, or audit this new ABI promise and add it to ` +
        `scripts/lint/graph-lite-exports.ts.`,
    });
  }

  return findings;
}
