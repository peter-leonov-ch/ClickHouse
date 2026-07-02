#!/usr/bin/env bash

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

# `EXPLAIN AST json = 1` named slots for CREATE/ALTER QUOTA, ROW POLICY,
# SETTINGS PROFILE and MASKING POLICY: ASTCreateQuotaQuery (incl. limits),
# ASTCreateRowPolicyQuery (incl. filters), ASTCreateSettingsProfileQuery,
# ASTCreateMaskingPolicyQuery, and ASTRowPolicyNames / ASTSettingsProfileElement.

ast() {
    $CLICKHOUSE_CLIENT --format TSVRaw -q "EXPLAIN AST json = 1 $1"
}

echo "-- CREATE QUOTA: key type, limits, roles"
ast "CREATE QUOTA q KEYED BY user_name FOR INTERVAL 1 HOUR MAX queries = 100, result_rows = 1000 TO r1" \
    | jq -c '.ast | {type, names, key_type, limits, roles}'

echo "-- ALTER QUOTA: rename, tracking-only interval"
ast "ALTER QUOTA q RENAME TO q2 FOR INTERVAL 1 DAY TRACKING ONLY" \
    | jq -c '.ast | {type, alter, new_name, limits}'

echo "-- CREATE ROW POLICY: restrictive filter"
ast "CREATE ROW POLICY p ON db.t AS restrictive FOR SELECT USING x > 0 TO r1" \
    | jq -c '.ast | {type, names, is_restrictive, filters, roles}'

echo "-- ALTER ROW POLICY: filter set to NONE"
ast "ALTER ROW POLICY p ON db.t USING NONE" \
    | jq -c '.ast | {type, alter, filters}'

echo "-- CREATE SETTINGS PROFILE"
ast "CREATE SETTINGS PROFILE sp SETTINGS max_threads = 8 MIN 1 MAX 16 TO ALL" \
    | jq -c '.ast | {type, names, settings, to_roles}'

echo "-- CREATE MASKING POLICY"
ast "CREATE MASKING POLICY m ON db.t UPDATE x = mask(x) WHERE y > 0 TO r1 PRIORITY 5" \
    | jq -c '.ast | {type, name, database, table, update_assignments, where_condition, roles, priority}'
