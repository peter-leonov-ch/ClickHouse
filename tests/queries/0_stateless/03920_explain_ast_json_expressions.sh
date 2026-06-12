#!/usr/bin/env bash

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

# Exercise `EXPLAIN AST json = 1` over a variety of expressions: literal value
# types, lambda functions, operators (which the parser normalizes to ordinary
# functions), CAST, the NULLS action, and compound / qualified identifiers.

ast() {
    $CLICKHOUSE_CLIENT --format TSVRaw -q "EXPLAIN AST json = 1 $1"
}

echo "-- literal value types"
ast "SELECT [1, 2, 3], (1, 'a'), map('k', 1), NULL, -5, 1.5, true, 'str'" \
    | jq -c '[.. | objects | select(.type == "Literal") | {value_type, value}]'

echo "-- lambda function: parameter tuple + body via arguments"
ast "SELECT arrayMap(x -> x + 1, arr)" \
    | jq -c '.. | objects | select(.name == "lambda")'

echo "-- operators normalize to functions"
ast "SELECT x IN (1, 2), y BETWEEN 1 AND 2, NOT z, a AND b OR c, arr[1], tup.2" \
    | jq -c '[.. | objects | select(.type == "Function") | {name, is_operator, args: (.arguments | length)}]'

echo "-- CAST forms"
ast "SELECT CAST(x AS Int64), y::String" \
    | jq -c '[.. | objects | select(.name == "CAST") | {name, is_operator, args: (.arguments | length)}]'

echo "-- NULLS action on aggregate"
ast "SELECT first_value(x) IGNORE NULLS FROM t" \
    | jq -c '.. | objects | select(.name == "first_value") | {name, nulls_action}'

echo "-- compound identifier and qualified table"
ast "SELECT db.tbl.col FROM db.tbl" \
    | jq -c '.. | objects | select(.type == "Identifier" or .type == "TableIdentifier")'
