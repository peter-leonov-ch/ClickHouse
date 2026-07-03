#!/usr/bin/env bash

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

# `EXPLAIN AST json = 1` named slots for the ALTER family (ASTAlterQuery /
# ASTAlterCommand) and the dictionary / CREATE FUNCTION nodes.

ast() {
    $CLICKHOUSE_CLIENT --format TSVRaw -q "EXPLAIN AST json = 1 $1"
}

echo "-- ALTER multi-command: command types and key slots"
ast "ALTER TABLE db.t ADD COLUMN x UInt8 AFTER y, DROP COLUMN z, ADD INDEX idx a TYPE minmax GRANULARITY 1, RENAME COLUMN p TO q" \
    | jq -c '.. | objects | select(.type == "AlterCommand") | {command_type, col: (.column_declaration.name // .column.name), rename_to: .rename_to.name}'

echo "-- ALTER UPDATE"
ast "ALTER TABLE t UPDATE a = 1 WHERE b = 2" \
    | jq -c '.. | objects | select(.type == "AlterCommand") | {command_type, assignments: [.assignments[].column], predicate: .predicate.name}'

echo "-- ALTER MOVE PARTITION"
ast "ALTER TABLE t MOVE PARTITION 1 TO DISK 'cold'" \
    | jq -c '.. | objects | select(.type == "AlterCommand") | {command_type, partition: .partition.type, move_destination_type, move_destination_name}'

echo "-- ALTER CLEAR STATISTICS (clear_statistics distinguishes it from DROP STATISTICS)"
ast "ALTER TABLE t CLEAR STATISTICS s" \
    | jq -c '.. | objects | select(.type == "AlterCommand") | {command_type, clear_statistics}'

echo "-- ALTER FREEZE PARTITION WITH NAME"
ast "ALTER TABLE t FREEZE PARTITION 1 WITH NAME 'backup1'" \
    | jq -c '.. | objects | select(.type == "AlterCommand") | {command_type, with_name}'

echo "-- ALTER REPLACE vs ATTACH PARTITION FROM (same command_type, differ by replace)"
ast "ALTER TABLE t REPLACE PARTITION 1 FROM src" \
    | jq -c '.. | objects | select(.type == "AlterCommand") | {command_type, replace, from_table}'
ast "ALTER TABLE t ATTACH PARTITION 1 FROM src" \
    | jq -c '.. | objects | select(.type == "AlterCommand") | {command_type, replace, from_table}'

echo "-- ALTER UNLOCK SNAPSHOT"
ast "ALTER TABLE t UNLOCK SNAPSHOT 'snap'" \
    | jq -c '.. | objects | select(.type == "AlterCommand") | {command_type, snapshot_name}'

echo "-- AlterQuery wrapper"
ast "ALTER TABLE t DROP COLUMN c" \
    | jq -c '.. | objects | select(.type == "AlterQuery") | {alter_object, table: .table.name, commands: [.commands[].command_type]}'

echo "-- CREATE FUNCTION"
ast "CREATE FUNCTION lin AS (x, k, b) -> k * x + b" \
    | jq -c '.. | objects | select(.type == "CreateFunctionQuery") | {name: .function_name.name, core: .function_core.name}'

echo "-- CREATE FUNCTION ON CLUSTER"
ast "CREATE FUNCTION IF NOT EXISTS lin ON CLUSTER 'c' AS (x) -> x" \
    | jq -c '.. | objects | select(.type == "CreateFunctionQuery") | {if_not_exists, cluster, name: .function_name.name}'

echo "-- DROP FUNCTION"
ast "DROP FUNCTION IF EXISTS lin ON CLUSTER 'c'" \
    | jq -c '.. | objects | select(.type == "DropFunctionQuery") | {function_name, if_exists, cluster}'

echo "-- CREATE DICTIONARY: attributes"
ast "CREATE DICTIONARY d (id UInt64, val String DEFAULT 'x' HIERARCHICAL) PRIMARY KEY id SOURCE(CLICKHOUSE(TABLE 't')) LAYOUT(HASHED()) LIFETIME(MIN 1 MAX 10)" \
    | jq -c '.. | objects | select(.type == "DictionaryAttributeDeclaration") | {name, data_type: .data_type.name, hierarchical}'

echo "-- CREATE DICTIONARY: source (key-value), layout, lifetime"
ast "CREATE DICTIONARY d (id UInt64) PRIMARY KEY id SOURCE(CLICKHOUSE(TABLE 't')) LAYOUT(HASHED()) LIFETIME(MIN 1 MAX 10)" \
    | jq -c '.. | objects | select(.type == "FunctionWithKeyValueArguments") | {name, elements: [.elements[] | {key, value: .value.value}]}'

echo "-- dictionary sub-elements get distinct type ids (not all 'Dictionary')"
ast "CREATE DICTIONARY d (id UInt64) PRIMARY KEY id SOURCE(CLICKHOUSE(TABLE 't')) LAYOUT(HASHED()) LIFETIME(MIN 1 MAX 10) RANGE(MIN s MAX e)" \
    | jq -c '[.. | objects | (.type // empty)] | map(select(test("^Dictionary")))'
