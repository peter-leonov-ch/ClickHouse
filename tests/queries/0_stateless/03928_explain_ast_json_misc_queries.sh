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

echo "-- KILL QUERY (SYNC)"
ast "KILL QUERY WHERE query_id = 'x' SYNC" | jq -c '.. | objects | select(.type == "KillQueryQuery") | {kill_type, sync, where: .where.name}'

echo "-- KILL MUTATION (TEST); ASYNC is sync=false/test=false"
ast "KILL MUTATION WHERE 1 TEST" | jq -c '.. | objects | select(.type == "KillQueryQuery") | {kill_type, sync: (.sync // false), test: (.test // false)}'

echo "-- transaction control: BEGIN / COMMIT / ROLLBACK / SET SNAPSHOT"
ast "BEGIN TRANSACTION" | jq -c '.. | objects | select(.type == "TransactionControl")'
ast "COMMIT" | jq -c '.. | objects | select(.type == "TransactionControl")'
ast "ROLLBACK" | jq -c '.. | objects | select(.type == "TransactionControl")'
ast "SET TRANSACTION SNAPSHOT 42" | jq -c '.. | objects | select(.type == "TransactionControl")'

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

echo "-- UNDROP TABLE with UUID and ON CLUSTER"
ast "UNDROP TABLE db.t UUID '00000000-0000-0000-0000-000000000abc' ON CLUSTER 'c'" \
    | jq -c '.. | objects | select(.type == "UndropQuery") | {database: .database.name, table: .table.name, uuid, cluster}'

echo "-- BACKUP TABLE: element rename, partitions, base_backup, cluster"
ast "BACKUP TABLE db.t AS db2.t2 PARTITIONS 1, 2 ON CLUSTER 'c' TO Disk('d', 'p') SETTINGS base_backup = Disk('d', 'base')" \
    | jq -c '.. | objects | select(.type == "BackupQuery") | {kind, cluster, elements, backup: .backup_name.name, base: .base_backup_name.name}'

echo "-- RESTORE DATABASE: rename and EXCEPT TABLES"
ast "RESTORE DATABASE db AS db2 EXCEPT TABLES x FROM Disk('d', 'p')" \
    | jq -c '.. | objects | select(.type == "RestoreQuery") | {kind, elements}'

echo "-- BACKUP ALL EXCEPT"
ast "BACKUP ALL EXCEPT DATABASES sys TO Disk('d', 'p')" \
    | jq -c '.. | objects | select(.type == "BackupQuery") | {kind, elements}'

echo "-- trailing SETTINGS clause captured for table-output / describe / kill / rename"
settings() { ast "$1" | jq -c '[.. | objects | select(.type == "Settings") | .changes] | add'; }
settings "DELETE FROM t WHERE x SETTINGS mutations_sync = 1"
settings "UPDATE t SET a = 1 WHERE b SETTINGS mutations_sync = 2"
settings "OPTIMIZE TABLE t SETTINGS optimize_throw_if_noop = 1"
settings "DESCRIBE TABLE t SETTINGS describe_compact_output = 1"
settings "KILL QUERY WHERE 1 SETTINGS max_threads = 3"
settings "RENAME TABLE a TO b SETTINGS distributed_ddl_task_timeout = 5"

echo "-- a Map-typed setting value is a JSON object (not an array of key/value tuples)"
settings "SELECT 1 SETTINGS additional_table_filters = {'t1': 'x > 1', 't2': 'y < 2'}"

echo "-- trailing FORMAT clause captured (table-output / create / show / describe / kill)"
fmt() { ast "$1" | jq -c '[.. | objects | select(has("format")) | .format] | add'; }
fmt "DROP TABLE t FORMAT Null"
fmt "CREATE TABLE t (a UInt8) ENGINE = Memory AS SELECT 1 FORMAT JSON"
fmt "SHOW TABLES FORMAT JSONEachRow"
fmt "DESCRIBE TABLE t FORMAT TSV"
fmt "KILL QUERY WHERE 1 FORMAT Null"

echo "-- trailing FORMAT captured for select-family / explain / access-entity SHOW"
fmt "SELECT 1 FORMAT JSON"
fmt "SELECT 1 UNION ALL SELECT 2 FORMAT TSV"
fmt "SELECT 1 INTERSECT SELECT 2 FORMAT JSONEachRow"
fmt "EXPLAIN PIPELINE SELECT 1 FORMAT JSON"
fmt "SHOW GRANTS FOR u FORMAT JSON"
fmt "SHOW CREATE USER u FORMAT JSON"
fmt "SHOW CREATE ROW POLICY p ON db.t FORMAT JSON"

echo "-- trailing INTO OUTFILE captured: filename, APPEND/TRUNCATE/AND STDOUT flags, COMPRESSION [LEVEL], and combined with FORMAT"
outfile() { ast "$1" | jq -c '.. | objects | select(has("out_file")) | {out_file: .out_file.value, outfile_append, outfile_truncate, outfile_with_stdout, compression: .compression.value, compression_level: .compression_level.value, format} | with_entries(select(.value != null))'; }
outfile "SELECT 1 INTO OUTFILE 'out.tsv'"
outfile "SELECT 1 INTO OUTFILE 'out.tsv' APPEND"
outfile "SELECT 1 INTO OUTFILE 'out.tsv' TRUNCATE"
outfile "SELECT 1 INTO OUTFILE 'out.tsv' AND STDOUT"
outfile "SELECT 1 INTO OUTFILE 'out.gz' COMPRESSION 'gz' LEVEL 3"
outfile "SELECT 1 INTO OUTFILE 'out.tsv' TRUNCATE FORMAT TSV"

echo "-- output-level SETTINGS (placed AFTER FORMAT) captured on the SelectWithUnionQuery wrapper"
wrap_settings() { ast "$1" | jq -c '.ast | {type, wrapper_settings: (.settings.changes // null), inner_settings: (.selects[0].settings.changes // null)}'; }
# SETTINGS after FORMAT -> wrapper (output-clause) settings
wrap_settings "SELECT 1 FORMAT TSV SETTINGS max_threads = 5"
# SETTINGS after FORMAT on a UNION -> wrapper settings
wrap_settings "SELECT 1 UNION ALL SELECT 2 FORMAT TSV SETTINGS max_threads = 5"
# SETTINGS before FORMAT -> consumed into the inner SelectQuery, not the wrapper
wrap_settings "SELECT 1 SETTINGS max_threads = 5 FORMAT TSV"
