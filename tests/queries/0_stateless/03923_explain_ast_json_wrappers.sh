#!/usr/bin/env bash

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

# `EXPLAIN AST json = 1` exposes the structural wrapper nodes through named
# slots too: table expressions, joins, array joins, subqueries, WITH elements,
# window definitions, and interpolate elements. `children` is left only on the
# homogeneous lists (ExpressionList, TablesInSelectQuery).

ast() {
    $CLICKHOUSE_CLIENT --format TSVRaw -q "EXPLAIN AST json = 1 $1"
}

echo "-- INNER JOIN ... ON"
ast "SELECT * FROM a INNER JOIN b ON a.x = b.x" | jq -c '.. | objects | select(.type == "TableJoin")'

echo "-- LEFT JOIN ... USING"
ast "SELECT * FROM a LEFT JOIN b USING (x, y)" | jq -c '.. | objects | select(.type == "TableJoin")'

echo "-- GLOBAL ANY join: strictness and locality"
ast "SELECT * FROM a GLOBAL ANY INNER JOIN b ON a.x = b.x" | jq -c '.. | objects | select(.type == "TableJoin") | {kind, strictness, locality}'

echo "-- CROSS and COMMA joins"
ast "SELECT * FROM a CROSS JOIN b" | jq -c '.. | objects | select(.type == "TableJoin") | .kind'
ast "SELECT * FROM a, b" | jq -c '[.. | objects | select(.type == "TableJoin") | .kind]'

echo "-- LEFT ARRAY JOIN"
ast "SELECT * FROM t LEFT ARRAY JOIN arr AS e, other" | jq -c '.. | objects | select(.type == "ArrayJoin")'

echo "-- table function"
ast "SELECT * FROM numbers(5)" | jq -c '.. | objects | select(.type == "TableExpression")'

echo "-- subquery in FROM: collapsed wrapper chain"
ast "SELECT * FROM (SELECT 1)" | jq -c '.. | objects | select(.type == "TableExpression")'

echo "-- table with FINAL and SAMPLE"
ast "SELECT * FROM tbl FINAL SAMPLE 0.1" | jq -c '.. | objects | select(.type == "TableExpression") | {database_and_table_name: .database_and_table_name.type, final, sample_size: .sample_size.type}'

echo "-- WITH element (named CTE): subquery collapses to query.selects"
ast "WITH cte AS (SELECT 1 AS a) SELECT a FROM cte" | jq -c '.. | objects | select(.type == "WithElement") | {name, query_select: .subquery.query.selects}'

echo "-- window definition with a RANGE frame"
ast "SELECT sum(x) OVER (PARTITION BY g ORDER BY b RANGE BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING) FROM t" \
    | jq -c '.. | objects | select(.type == "WindowDefinition")'

echo "-- UNION: selects inlined (no ExpressionList wrapper)"
ast "SELECT 1 UNION ALL SELECT 2" | jq -c '.. | objects | select(.type == "SelectWithUnionQuery") | [.selects[].type]'

echo "-- INTERPOLATE element"
ast "SELECT x FROM t ORDER BY x WITH FILL INTERPOLATE (y AS y + 1)" | jq -c '.. | objects | select(.type == "InterpolateElement")'
