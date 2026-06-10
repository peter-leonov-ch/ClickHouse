#!/usr/bin/env bash

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

# `EXPLAIN AST json = 1` for set operations. `ASTSelectWithUnionQuery` reports a
# non-default `union_mode`; `ASTSelectIntersectExceptQuery` derives from
# `ASTSelectQuery` but keeps its operand selects in `children`, so they must
# still be emitted (regression guard) alongside its `operator`.

ast() {
    $CLICKHOUSE_CLIENT --format TSVRaw -q "EXPLAIN AST json = 1 $1"
}

echo "-- UNION ALL: default mode, no union_mode field"
ast "SELECT 1 UNION ALL SELECT 2" \
    | jq -c '.. | objects | select(.type == "SelectWithUnionQuery") | {type, union_mode}'

echo "-- UNION DISTINCT: explicit mode"
ast "SELECT 1 UNION DISTINCT SELECT 2" \
    | jq -c '.. | objects | select(.type == "SelectWithUnionQuery") | {type, union_mode}'

echo "-- INTERSECT: operator plus both operand selects"
ast "SELECT 1 INTERSECT SELECT 2" \
    | jq -c '.. | objects | select(.type == "SelectIntersectExceptQuery") | {operator, operands: [.children[].type]}'

echo "-- EXCEPT: operator plus both operand selects"
ast "SELECT 1 EXCEPT SELECT 2" \
    | jq -c '.. | objects | select(.type == "SelectIntersectExceptQuery") | {operator, operands: [.children[].type]}'
