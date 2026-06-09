#!/usr/bin/env bash

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

# `EXPLAIN AST json = 1` exposes ASTOrderByElement sub-nodes through named
# slots (`expression`, `collation`, `fill_from`, `fill_to`, `fill_step`,
# `fill_staleness`) instead of a positional `children` array.

explain_order_by() {
    $CLICKHOUSE_CLIENT --format TSVRaw -q "EXPLAIN AST json = 1 $1" \
        | jq -c '.. | objects | select(.type == "OrderByElement")'
}

echo "-- direction and explicit nulls"
explain_order_by "SELECT x FROM t ORDER BY a DESC NULLS FIRST"

echo "-- collation"
explain_order_by "SELECT x FROM t ORDER BY b ASC COLLATE 'en'"

echo "-- with fill bounds"
explain_order_by "SELECT x FROM t ORDER BY c WITH FILL FROM 1 TO 10 STEP 2"
