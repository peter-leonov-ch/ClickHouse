#!/usr/bin/env bash

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

# Leaf-ish nodes that used to render as a bare `{"type": ...}` now expose their
# internal state: the SETTINGS clause (ASTSetQuery), SAMPLE ratio
# (ASTSampleRatio), asterisks with column transformers, and the COLUMNS
# matchers. `children` is left on the homogeneous column lists.

ast() {
    $CLICKHOUSE_CLIENT --format TSVRaw -q "EXPLAIN AST json = 1 $1"
}

echo "-- SETTINGS clause: changes as name -> value"
ast "SELECT 1 SETTINGS max_threads = 4, max_block_size = 1000" | jq -c '.. | objects | select(.type == "Set")'

echo "-- SAMPLE ratio"
ast "SELECT * FROM tbl SAMPLE 1/10" | jq -c '.. | objects | select(.type == "SampleRatio")'

echo "-- plain asterisk stays empty"
ast "SELECT * FROM t" | jq -c '.. | objects | select(.type == "Asterisk")'

echo "-- asterisk with EXCEPT transformer"
ast "SELECT * EXCEPT (a, b) FROM t" | jq -c '.. | objects | select(.type == "Asterisk")'

echo "-- asterisk with APPLY transformer"
ast "SELECT * APPLY(sum) FROM t" | jq -c '.. | objects | select(.type == "ColumnsApplyTransformer")'

echo "-- asterisk with REPLACE transformer"
ast "SELECT * REPLACE (x + 1 AS y) FROM t" | jq -c '.. | objects | select(.type == "ColumnsReplaceTransformer::Replacement")'

echo "-- qualified asterisk"
ast "SELECT t.* FROM t" | jq -c '.. | objects | select(.type == "QualifiedAsterisk")'

echo "-- COLUMNS regexp matcher"
ast "SELECT COLUMNS('a.*') FROM t" | jq -c '.. | objects | select(.type == "ColumnsRegexpMatcher")'

echo "-- COLUMNS list matcher"
ast "SELECT COLUMNS(a, b) FROM t" | jq -c '.. | objects | select(.type == "ColumnsListMatcher")'
