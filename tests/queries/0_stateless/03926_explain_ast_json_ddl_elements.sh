#!/usr/bin/env bash

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

# `EXPLAIN AST json = 1` named slots for CREATE-TABLE sub-elements (Index,
# Constraint, Projection, ProjectionSelectQuery, TTLElement, Partition) and the
# lightweight DML / table-op statements (DELETE, UPDATE, DROP, OPTIMIZE,
# TRUNCATE, Assignment).

ast() {
    $CLICKHOUSE_CLIENT --format TSVRaw -q "EXPLAIN AST json = 1 $1"
}

echo "-- INDEX declaration"
ast "CREATE TABLE t (a UInt64, INDEX idx a TYPE minmax GRANULARITY 4) ENGINE = MergeTree ORDER BY a" \
    | jq -c '.. | objects | select(.type == "Index")'

echo "-- CONSTRAINT declaration"
ast "CREATE TABLE t (a UInt64, CONSTRAINT c CHECK a > 0) ENGINE = MergeTree ORDER BY a" \
    | jq -c '.. | objects | select(.type == "Constraint") | {name, constraint_type, expr: .expression.name}'

echo "-- PROJECTION declaration"
ast "CREATE TABLE t (a UInt64, b String, PROJECTION p (SELECT a GROUP BY a)) ENGINE = MergeTree ORDER BY a" \
    | jq -c '.. | objects | select(.type == "ProjectionSelectQuery")'

echo "-- PROJECTION INDEX ... TYPE ... WITH SETTINGS"
ast "ALTER TABLE t ADD PROJECTION p INDEX a TYPE minmax WITH SETTINGS (x = 1)" \
    | jq -c '.. | objects | select(.type == "Projection") | {name, index: .index.name, index_type: .index_type.name, settings: .settings.changes}'

echo "-- TTL DELETE"
ast "CREATE TABLE t (a UInt64, d Date) ENGINE = MergeTree ORDER BY a TTL d + INTERVAL 1 DAY" \
    | jq -c '.. | objects | select(.type == "TTLElement")'

echo "-- TTL MOVE TO DISK"
ast "ALTER TABLE t MODIFY TTL d + INTERVAL 1 MONTH TO DISK 'cold'" \
    | jq -c '.. | objects | select(.type == "TTLElement") | {mode, destination_type, destination_name}'

echo "-- TTL GROUP BY ... SET ..."
ast "CREATE TABLE t (a UInt64, k UInt64, d Date) ENGINE = MergeTree ORDER BY (k, a) TTL d + INTERVAL 1 MONTH GROUP BY k SET a = max(a)" \
    | jq -c '.. | objects | select(.type == "TTLElement") | {mode, group_by_key: [.group_by_key[].name], assignments: [.group_by_assignments[].column]}'

echo "-- DELETE"
ast "DELETE FROM t WHERE x > 1" \
    | jq -c '.. | objects | select(.type == "DeleteQuery") | {table: .table.name, predicate: .predicate.name}'

echo "-- UPDATE with assignments"
ast "UPDATE t SET a = 1, b = b + 1 WHERE c > 0" \
    | jq -c '.. | objects | select(.type == "UpdateQuery") | {table: .table.name, assignments: [.assignments[].column]}'

echo "-- DROP with flags"
ast "DROP TABLE IF EXISTS db.t SYNC" \
    | jq -c '.. | objects | select(.type == "DropQuery") | {kind, database: .database.name, table: .table.name, if_exists, sync}'

echo "-- TRUNCATE (drop kind)"
ast "TRUNCATE TABLE t" \
    | jq -c '.. | objects | select(.type == "TruncateQuery") | {kind, table: .table.name}'

echo "-- TRUNCATE ALL TABLES FROM (with LIKE pattern)"
ast "TRUNCATE ALL TABLES FROM db NOT ILIKE 'foo%'" \
    | jq -c '.. | objects | select(.type == "TruncateQuery") | {kind, database: .database.name, has_all, has_tables, like, not_like, case_insensitive_like}'

echo "-- OPTIMIZE with partition"
ast "OPTIMIZE TABLE t PARTITION 7 FINAL DEDUPLICATE" \
    | jq -c '.. | objects | select(.type == "OptimizeQuery") | {table: .table.name, partition: .partition.type, final, deduplicate}'
