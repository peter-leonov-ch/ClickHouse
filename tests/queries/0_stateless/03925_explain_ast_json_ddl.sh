#!/usr/bin/env bash

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

# `EXPLAIN AST json = 1` named slots for the CREATE-TABLE family and INSERT:
# ASTCreateQuery, ASTColumns, ASTColumnDeclaration, ASTDataType, ASTStorage,
# ASTInsertQuery.

ast() {
    $CLICKHOUSE_CLIENT --format TSVRaw -q "EXPLAIN AST json = 1 $1"
}

echo "-- CREATE TABLE: columns, types, defaults, codec, comment"
ast "CREATE TABLE db.t (id UInt64, s String DEFAULT 'x' CODEC(ZSTD), n Decimal(10, 2) COMMENT 'c') ENGINE = MergeTree ORDER BY id" \
    | jq -c '.. | objects | select(.type == "ColumnDeclaration")'

echo "-- storage"
ast "CREATE TABLE t (id UInt64) ENGINE = MergeTree PARTITION BY toYYYYMM(d) ORDER BY id SETTINGS index_granularity = 8192" \
    | jq -c '.. | objects | select(.type == "Storage")'

echo "-- nested data types"
ast "CREATE TABLE t (a Array(String), m Map(String, UInt8))" \
    | jq -c '[.. | objects | select(.type == "DataType") | {name, args: (.arguments | length)}]'

echo "-- CREATE flags and target"
ast "CREATE MATERIALIZED VIEW IF NOT EXISTS mv ENGINE = Log AS SELECT * FROM src" \
    | jq -c '.. | objects | select(.type == "CreateQuery") | {is_materialized_view, if_not_exists, table: .table.name, select: .select.type}'

echo "-- CREATE TABLE AS"
ast "CREATE TABLE t AS other" \
    | jq -c '.. | objects | select(.type == "CreateQuery") | {table: .table.name, as_table}'

echo "-- INSERT ... SELECT"
ast "INSERT INTO t (a, b) SELECT 1, 2" \
    | jq -c '.. | objects | select(.type == "InsertQuery") | {table: .table.name, columns: [.columns[].name], select: .select.type}'

echo "-- INSERT ... VALUES"
ast "INSERT INTO db.t VALUES (1, 2)" \
    | jq -c '.. | objects | select(.type == "InsertQuery") | {database: .database.name, table: .table.name}'
