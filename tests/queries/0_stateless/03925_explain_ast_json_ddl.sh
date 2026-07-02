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

echo "-- ATTACH: if_not_exists, uuid, cluster"
ast "ATTACH TABLE IF NOT EXISTS db.t UUID '00000000-0000-0000-0000-000000000abc' ON CLUSTER 'c'" \
    | jq -c '.. | objects | select(.type == "AttachQuery") | {attach, if_not_exists, uuid, cluster, table: .table.name}'

echo "-- ATTACH ... FROM path"
ast "ATTACH TABLE db.t FROM '/var/lib/p'" \
    | jq -c '.. | objects | select(.type == "AttachQuery") | {attach, attach_from_path, table: .table.name}'

echo "-- ATTACH ... AS NOT REPLICATED conversion marker"
ast "ATTACH TABLE db.t AS NOT REPLICATED" \
    | jq -c '.. | objects | select(.type == "AttachQuery") | {attach, attach_as_replicated, table: .table.name}'

echo "-- Enum data type: explicit value pairs"
ast "CREATE TABLE t (e Enum8('a' = 1, 'b' = 2)) ENGINE = Memory" \
    | jq -c '.. | objects | select(.type == "EnumDataType")'

echo "-- Tuple data type: named element names"
ast "CREATE TABLE t (x Tuple(a UInt8, b String)) ENGINE = Memory" \
    | jq -c '.. | objects | select(.type == "TupleDataType")'

echo "-- Tuple data type: unnamed (no element_names)"
ast "CREATE TABLE t (x Tuple(UInt8, String)) ENGINE = Memory" \
    | jq -c '.. | objects | select(.type == "TupleDataType")'

echo "-- column COLLATE"
ast "CREATE TABLE t (s String COLLATE binary) ENGINE = Memory" \
    | jq -c '.. | objects | select(.type == "Collation")'

echo "-- JSON typed path / skip / parameter arguments"
ast "CREATE TABLE t (j JSON(a.b UInt32, SKIP x, SKIP REGEXP 'y.*', max_dynamic_paths = 8)) ENGINE = Memory" \
    | jq -c '.. | objects | select(.type == "ObjectTypeArgument" or .type == "ObjectTypedPath")'

echo "-- refreshable materialized view"
ast "CREATE MATERIALIZED VIEW mv REFRESH EVERY 1 DAY OFFSET 1 HOUR APPEND (a UInt64) ENGINE = Memory AS SELECT 1 AS a" \
    | jq -c '.. | objects | select(.type == "RefreshStrategy" or .type == "TimeInterval")'

echo "-- INSERT FROM INFILE ... COMPRESSION"
ast "INSERT INTO t FROM INFILE 'data.csv' COMPRESSION 'gzip' FORMAT CSV" \
    | jq -c '.. | objects | select(.type == "InsertQuery") | {table: .table.name, infile: .infile.value, compression: .compression.value, format}'
