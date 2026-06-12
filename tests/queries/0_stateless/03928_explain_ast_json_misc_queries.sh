#!/usr/bin/env bash

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

# `EXPLAIN AST json = 1` named slots for the long tail: EXPLAIN, DESCRIBE,
# SHOW TABLES, CREATE/DROP INDEX, CHECK, USE, KILL, RENAME, SYSTEM, the
# qualified COLUMNS matchers, and the simple table-target statements
# (EXISTS / SHOW CREATE) via the generic fallback.

ast() {
    $CLICKHOUSE_CLIENT --format TSVRaw -q "EXPLAIN AST json = 1 $1"
}

echo "-- EXPLAIN (nested)"
ast "EXPLAIN SYNTAX SELECT 1" | jq -c '.. | objects | select(.type == "Explain") | {kind, query: .query.type}'

echo "-- DESCRIBE"
ast "DESCRIBE TABLE t" | jq -c '.. | objects | select(.type == "DescribeQuery") | {table_expression: .table_expression.type}'

echo "-- SHOW TABLES"
ast "SHOW TABLES FROM db LIKE 'a%'" | jq -c '.. | objects | select(.type == "ShowTables") | {from: .from.name, like}'

echo "-- CREATE INDEX"
ast "CREATE INDEX idx ON t (a) TYPE minmax GRANULARITY 1" \
    | jq -c '.. | objects | select(.type == "CreateIndexQuery") | {table: .table.name, index_name: .index_name.name, decl: .index_declaration.type}'

echo "-- DROP INDEX"
ast "DROP INDEX idx ON t" | jq -c '.. | objects | select(.type == "DropIndexQuery") | {table: .table.name, index_name: .index_name.name}'

echo "-- CHECK TABLE"
ast "CHECK TABLE t PART 'p1'" | jq -c '.. | objects | select(.type == "CheckQuery") | {table: .table.name, part_name}'

echo "-- USE"
ast "USE mydb" | jq -c '.. | objects | select(.type == "UseQuery") | {database: .database.name}'

echo "-- KILL QUERY"
ast "KILL QUERY WHERE query_id = 'x' SYNC" | jq -c '.. | objects | select(.type == "KillQueryQuery") | {kill_type, sync, where: .where.name}'

echo "-- RENAME"
ast "RENAME TABLE a TO b, c TO d" | jq -c '.. | objects | select(.type == "Rename") | .elements'

echo "-- SYSTEM"
ast "SYSTEM RELOAD DICTIONARY db.dict" | jq -c '.. | objects | select(.type == "SYSTEM") | {system_type, database: .database.name, table: .table.name}'

echo "-- qualified COLUMNS matcher"
ast "SELECT t.COLUMNS('a.*') FROM t" | jq -c '.. | objects | select(.type == "QualifiedColumnsRegexpMatcher") | {pattern, qualifier: .qualifier.name}'

echo "-- EXISTS (generic table-target fallback)"
ast "EXISTS TABLE db.t" | jq -c '.. | objects | select(.type == "ExistsTableQuery") | {database: .database.name, table: .table.name}'

echo "-- SHOW CREATE (generic table-target fallback)"
ast "SHOW CREATE TABLE db.t" | jq -c '.. | objects | select(.type == "ShowCreateTableQuery") | {database: .database.name, table: .table.name}'
