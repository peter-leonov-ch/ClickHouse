#!/usr/bin/env bash

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

# `EXPLAIN AST json = 1` named slots for the remaining access statements:
# ASTDropAccessEntityQuery, ASTMoveAccessEntityQuery, ASTShowGrantsQuery,
# ASTShowCreateAccessEntityQuery, ASTShowAccessEntitiesQuery, ASTExecuteAsQuery.

ast() {
    $CLICKHOUSE_CLIENT --format TSVRaw -q "EXPLAIN AST json = 1 $1"
}

echo "-- DROP USER"
ast "DROP USER IF EXISTS a, b" \
    | jq -c '.ast | {type, entity_type, if_exists, names}'

echo "-- DROP ROW POLICY"
ast "DROP ROW POLICY p ON db.t" \
    | jq -c '.ast | {type, entity_type, row_policy_names}'

echo "-- MOVE USER TO storage"
ast "MOVE USER u TO disk_storage" \
    | jq -c '.ast | {type, entity_type, storage_name, names}'

echo "-- SHOW GRANTS"
ast "SHOW GRANTS FOR u WITH IMPLICIT" \
    | jq -c '.ast | {type, for_roles, with_implicit}'

echo "-- SHOW CREATE USER"
ast "SHOW CREATE USER u" \
    | jq -c '.ast | {type, entity_type, names}'

echo "-- SHOW CREATE ROW POLICY"
ast "SHOW CREATE ROW POLICY p ON db.t" \
    | jq -c '.ast | {type, entity_type, row_policy_names}'

echo "-- SHOW USERS"
ast "SHOW USERS" \
    | jq -c '.ast | {type, entity_type, all}'

echo "-- SHOW QUOTAS"
ast "SHOW QUOTAS" \
    | jq -c '.ast | {type, entity_type}'

echo "-- EXECUTE AS"
ast "EXECUTE AS u" \
    | jq -c '.ast | {type, target_user}'
