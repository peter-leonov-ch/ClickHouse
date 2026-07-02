#!/usr/bin/env bash

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

# `EXPLAIN AST json = 1` named slots for CREATE/ALTER USER and ROLE and
# SET ROLE: ASTCreateUserQuery, ASTCreateRoleQuery, ASTSetRoleQuery, plus the
# helper nodes ASTUserNamesWithHost, ASTAuthenticationData, ASTRolesOrUsersSet,
# ASTSettingsProfileElements, ASTDatabaseOrNone, ASTPublicSSHKey.

ast() {
    $CLICKHOUSE_CLIENT --format TSVRaw -q "EXPLAIN AST json = 1 $1"
}

echo "-- CREATE USER: name, auth, host, default role, settings, grantees"
ast "CREATE USER u IDENTIFIED WITH sha256_password BY 'secret' HOST IP '127.0.0.1' DEFAULT ROLE r1 SETTINGS max_threads = 4 READONLY GRANTEES ANY" \
    | jq -c '.ast | {type, names, authentication_methods, hosts, default_roles, settings, grantees}'

echo "-- CREATE USER: name@host"
ast "CREATE USER u2@'192.168.%'" \
    | jq -c '.ast.names'

echo "-- auth: ssl_certificate"
ast "CREATE USER u IDENTIFIED WITH ssl_certificate CN 'cn1', 'cn2'" \
    | jq -c '.ast.authentication_methods'

echo "-- auth: ssh_key (PublicSSHKey)"
ast "CREATE USER u IDENTIFIED WITH ssh_key BY KEY 'AAAA' TYPE 'ssh-rsa'" \
    | jq -c '.ast.authentication_methods'

echo "-- auth: kerberos"
ast "CREATE USER u IDENTIFIED WITH kerberos REALM 'r'" \
    | jq -c '.ast.authentication_methods'

echo "-- default database NONE"
ast "CREATE USER u DEFAULT DATABASE NONE" \
    | jq -c '.ast.default_database'

echo "-- ALTER USER: rename, add host, drop settings"
ast "ALTER USER u RENAME TO u2 ADD HOST IP '10.0.0.0/8' DROP SETTINGS max_threads" \
    | jq -c '.ast | {type, alter, new_name, add_hosts, alter_settings}'

echo "-- CREATE ROLE"
ast "CREATE ROLE OR REPLACE r SETTINGS max_threads = 2" \
    | jq -c '.ast | {type, or_replace, names, settings}'

echo "-- ALTER ROLE"
ast "ALTER ROLE r RENAME TO r2" \
    | jq -c '.ast | {type, alter, names, new_name}'

echo "-- SET ROLE"
ast "SET ROLE r1, r2" \
    | jq -c '.ast | {type, kind, roles}'

echo "-- SET DEFAULT ROLE"
ast "SET DEFAULT ROLE r1 TO u1, u2" \
    | jq -c '.ast | {type, kind, roles, to_users}'
