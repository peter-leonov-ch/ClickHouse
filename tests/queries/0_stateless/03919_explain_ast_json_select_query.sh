#!/usr/bin/env bash

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

# `EXPLAIN AST json = 1` exposes ASTSelectQuery clauses through named slots
# (`with`, `select`, `tables`, `where`, `group_by`, `order_by`, ...) instead of
# a positional `children` array. List-shaped clauses inline their inner
# `ExpressionList` wrapper.

select_query() {
    $CLICKHOUSE_CLIENT --format TSVRaw -q "EXPLAIN AST json = 1 $1" \
        | jq -c '.. | objects | select(.type == "SelectQuery") | '"$2"
}

echo "-- all clauses: slot names present, in order"
select_query "SELECT a FROM t PREWHERE p WHERE w GROUP BY g HAVING h QUALIFY q ORDER BY o LIMIT 5, 10 BY b SETTINGS max_threads = 1" "keys_unsorted"

echo "-- minimal"
select_query "SELECT 1" "."

echo "-- WITH / CTE list"
select_query "WITH 1 AS x SELECT x" ".with"

echo "-- WINDOW list"
select_query "SELECT sum(a) OVER w FROM t WINDOW w AS (PARTITION BY b)" ".window"

echo "-- single-expression clauses are objects, list clauses are arrays"
select_query "SELECT a FROM t WHERE w GROUP BY g" "{where, group_by}"
