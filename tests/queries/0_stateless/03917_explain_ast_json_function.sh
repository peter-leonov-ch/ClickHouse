#!/usr/bin/env bash

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

# `EXPLAIN AST json = 1` exposes ASTFunction sub-nodes through named slots
# (`arguments`, `parameters`, `window_definition`) instead of a positional
# `children` array. Print the Function nodes with jq to keep the references
# focused on the relevant subtree.

explain_function() {
    $CLICKHOUSE_CLIENT --format TSVRaw -q "EXPLAIN AST json = 1 $1" \
        | jq -c '.. | objects | select(.type == "Function")'
}

echo "-- plain call"
explain_function "SELECT f(x, y)"

echo "-- parametric aggregate"
explain_function "SELECT quantile(0.9)(x)"

echo "-- windowed via name"
explain_function "SELECT sum(x) OVER w FROM t WINDOW w AS (PARTITION BY y)"

echo "-- windowed via inline definition"
explain_function "SELECT sum(x) OVER (PARTITION BY y) FROM t"

echo "-- operator"
explain_function "SELECT a + b"

echo "-- no arguments"
explain_function "SELECT now()"
