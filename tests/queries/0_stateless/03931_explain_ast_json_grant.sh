#!/usr/bin/env bash

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

# `EXPLAIN AST json = 1` named slots for GRANT / REVOKE / CHECK GRANT:
# ASTGrantQuery (split into GrantQuery / RevokeQuery), ASTCheckGrantQuery,
# the inlined AccessRightsElements privilege list, and ASTRolesOrUsersSet.

ast() {
    $CLICKHOUSE_CLIENT --format TSVRaw -q "EXPLAIN AST json = 1 $1"
}

echo "-- GRANT privileges with columns and grant option"
ast "GRANT SELECT(x, y), INSERT ON db.t TO user1 WITH GRANT OPTION" \
    | jq -c '.ast | {type, access_rights, grantees}'

echo "-- GRANT role to users"
ast "GRANT role1, role2 TO user1, user2 WITH ADMIN OPTION" \
    | jq -c '.ast | {type, admin_option, roles, grantees}'

echo "-- GRANT on all"
ast "GRANT SELECT ON *.* TO user1" \
    | jq -c '.ast.access_rights'

echo "-- REVOKE"
ast "REVOKE SELECT ON db.t FROM user1" \
    | jq -c '.ast | {type, access_rights, grantees}'

echo "-- REVOKE from ALL EXCEPT"
ast "REVOKE INSERT ON db.* FROM ALL EXCEPT user1" \
    | jq -c '.ast.grantees'

echo "-- GRANT CURRENT GRANTS"
ast "GRANT CURRENT GRANTS ON db.* TO user1" \
    | jq -c '.ast | {type, current_grants}'

echo "-- CHECK GRANT"
ast "CHECK GRANT SELECT(x), UPDATE ON db.t" \
    | jq -c '.ast | {type, access_rights}'
