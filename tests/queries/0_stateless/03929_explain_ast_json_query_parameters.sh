#!/usr/bin/env bash

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

# Query parameters `{name:type}` survive substitution only inside a
# parameterized view, so EXPLAIN AST json = 1 over a CREATE VIEW is how the
# ASTQueryParameter nodes reach the JSON. They appear both in value position
# and in identifier/table position.

ast() {
    $CLICKHOUSE_CLIENT --format TSVRaw -q "EXPLAIN AST json = 1 $1"
}

echo "-- top-level document carries a format version"
ast "SELECT 1" | jq -c '{version, ast_type: .ast.type}'

echo "-- query parameters in value and identifier/table position"
ast "CREATE VIEW v AS SELECT id FROM {tbl:Identifier} WHERE id = {x:UInt64}" \
    | jq -c '[.. | objects | select(.type == "QueryParameter") | {name, param_type}]'
