#!/usr/bin/env bash
# Regenerate the SQL -> JSON-AST fixture corpus.
#
# For every case it writes a pair under cases/:
#   <name>.sql   the input query (one statement, source of truth)
#   <name>.json  the expected `EXPLAIN AST json = 1` output (golden)
#
# Usage:
#   CLICKHOUSE_BINARY=/path/to/clickhouse ./generate.sh
# Defaults to `clickhouse` on PATH.

set -euo pipefail

BIN="${CLICKHOUSE_BINARY:-clickhouse}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="$HERE/cases"

rm -rf "$OUT"
mkdir -p "$OUT"

emit() {
    local name="$1" sql="$2"
    printf '%s\n' "$sql" > "$OUT/$name.sql"
    "$BIN" local --format TSVRaw -q "EXPLAIN AST json = 1 $sql" > "$OUT/$name.json"
    echo "  $name"
}

echo "Generating fixtures into $OUT"

# --- literals and scalars ---
emit 01_literals          "SELECT 1, -2, 3.5, 'str', NULL, true, [1, 2], (1, 'a'), map('k', 1)"
emit 02_identifiers       "SELECT a, t.b, db.tbl.c"

# --- functions and operators ---
emit 03_function_call     "SELECT f(x, y)"
emit 04_no_args           "SELECT now()"
emit 05_operators         "SELECT a + b, x IN (1, 2), y BETWEEN 1 AND 2, NOT z, p AND q OR r"
emit 06_subscript         "SELECT arr[1], tup.2"
emit 07_cast              "SELECT CAST(x AS Int64), y::String"
emit 08_lambda            "SELECT arrayMap(e -> e * 2, arr)"
emit 09_parametric_agg    "SELECT quantile(0.9)(x)"
emit 10_nulls_action      "SELECT first_value(x) IGNORE NULLS FROM t"

# --- window functions ---
emit 11_window_named      "SELECT sum(x) OVER w FROM t WINDOW w AS (PARTITION BY g ORDER BY x)"
emit 12_window_inline     "SELECT sum(x) OVER (PARTITION BY g ORDER BY x ROWS BETWEEN 1 PRECEDING AND CURRENT ROW) FROM t"

# --- select clauses ---
emit 13_select_star       "SELECT * FROM foo WHERE x = 1"
emit 14_distinct          "SELECT DISTINCT a, b FROM t"
emit 15_group_by_rollup   "SELECT g, count() FROM t GROUP BY g WITH ROLLUP HAVING count() > 1"
emit 16_grouping_sets     "SELECT x FROM t GROUP BY GROUPING SETS ((x), (y))"
emit 17_qualify           "SELECT x, row_number() OVER (ORDER BY x) AS r FROM t QUALIFY r = 1"
emit 18_order_by_fill     "SELECT x FROM t ORDER BY x DESC NULLS LAST WITH FILL FROM 0 TO 10 STEP 2 INTERPOLATE (x AS x + 1)"
# A multi-key list and a single tuple-valued key sort rows identically but are
# distinct ASTs: two OrderByElement nodes vs one whose expression is tuple(...).
emit 18b_order_by_multikey  "SELECT x FROM t ORDER BY a, b"
emit 18c_order_by_tuple_key "SELECT x FROM t ORDER BY tuple(a, b)"
emit 19_limits            "SELECT x FROM t ORDER BY x LIMIT 5 BY g LIMIT 100 OFFSET 10"
emit 19b_limit_with_ties  "SELECT x FROM t ORDER BY x DESC LIMIT 3 WITH TIES"
emit 20_settings          "SELECT 1 SETTINGS max_threads = 4, max_block_size = 1000"

# --- FROM / JOIN ---
emit 21_join_on           "SELECT * FROM a INNER JOIN b ON a.x = b.x"
emit 22_join_using        "SELECT * FROM a LEFT JOIN b USING (x, y)"
emit 23_join_global_any   "SELECT * FROM a GLOBAL ANY INNER JOIN b ON a.x = b.x"
emit 24_cross_comma       "SELECT * FROM a CROSS JOIN b, c"
emit 25_array_join        "SELECT * FROM t LEFT ARRAY JOIN arr AS e, other"
emit 26_table_function    "SELECT * FROM numbers(5)"
emit 27_subquery_from     "SELECT * FROM (SELECT 1 AS a)"
emit 28_sample_final      "SELECT * FROM tbl FINAL SAMPLE 1/10"

# --- WITH / set operations ---
emit 29_cte               "WITH cte AS (SELECT 1 AS a) SELECT a FROM cte"
emit 30_scalar_with       "WITH (SELECT max(n) FROM t) AS m SELECT m"
emit 31_union_all         "SELECT 1 UNION ALL SELECT 2"
emit 32_union_distinct    "SELECT 1 UNION DISTINCT SELECT 2"
emit 33_intersect         "SELECT 1 INTERSECT SELECT 2"
emit 34_except            "SELECT 1 EXCEPT SELECT 2"

# --- asterisk transformers and COLUMNS ---
emit 35_qualified_star    "SELECT t.* FROM t"
emit 36_except_transform  "SELECT * EXCEPT (a, b) FROM t"
emit 36b_except_pattern   "SELECT * EXCEPT 'a.*' FROM t"
emit 37_apply_transform   "SELECT * APPLY(sum) FROM t"
emit 38_replace_transform "SELECT * REPLACE (x + 1 AS y) FROM t"
emit 39_columns_regexp    "SELECT COLUMNS('a.*') FROM t"
emit 40_columns_list      "SELECT COLUMNS(a, b) FROM t"

# --- ALTER commands (fields the SQL formatter emits but are easy to drop) ---
emit 42_alter_move_partition       "ALTER TABLE t MOVE PARTITION 'p' TO DISK 'fast'"
emit 43_alter_clear_statistics     "ALTER TABLE t CLEAR STATISTICS s"
emit 44_alter_freeze_with_name     "ALTER TABLE t FREEZE PARTITION 'p' WITH NAME 'backup1'"
# 45 vs 45b: REPLACE and ATTACH ... FROM share command_type REPLACE_PARTITION and
# differ only by the `replace` flag.
emit 45_alter_replace_partition    "ALTER TABLE t REPLACE PARTITION 'p' FROM src"
emit 45b_alter_attach_partition    "ALTER TABLE t ATTACH PARTITION 'p' FROM src"
emit 46_alter_unlock_snapshot      "ALTER TABLE t UNLOCK SNAPSHOT 'snap'"

# --- access management ---
emit 47_grant             "GRANT SELECT(x, y), INSERT ON db.t TO user1 WITH GRANT OPTION"
emit 48_grant_role        "GRANT role1 TO user1 WITH ADMIN OPTION"
emit 49_revoke            "REVOKE SELECT ON db.t FROM ALL EXCEPT user1"
emit 50_create_user       "CREATE USER u IDENTIFIED WITH sha256_password BY 'secret' HOST IP '127.0.0.1' DEFAULT ROLE r1 SETTINGS max_threads = 4 READONLY GRANTEES ANY"
emit 51_alter_user        "ALTER USER u RENAME TO u2 ADD HOST IP '10.0.0.0/8' DROP SETTINGS max_threads"
emit 52_create_role       "CREATE ROLE r SETTINGS max_threads = 2"
emit 53_set_default_role  "SET DEFAULT ROLE r1 TO u1, u2"
emit 54_create_quota      "CREATE QUOTA q KEYED BY user_name FOR INTERVAL 1 HOUR MAX queries = 100, result_rows = 1000 TO r1"
emit 55_row_policy        "CREATE ROW POLICY p ON db.t AS restrictive FOR SELECT USING x > 0 TO r1"
emit 56_settings_profile  "CREATE SETTINGS PROFILE sp SETTINGS max_threads = 8 MIN 1 MAX 16 TO ALL"
emit 57_drop_user         "DROP USER IF EXISTS a, b"
emit 58_show_grants       "SHOW GRANTS FOR u WITH IMPLICIT"

# --- data types and table elements whose detail the native AST drops ---
emit 59_enum_data_type    "CREATE TABLE t (e Enum8('a' = 1, 'b' = 2)) ENGINE = Memory"
emit 60_tuple_named       "CREATE TABLE t (x Tuple(a UInt8, b String)) ENGINE = Memory"
emit 61_column_collate    "CREATE TABLE t (s String COLLATE binary) ENGINE = Memory"
emit 62_json_typed_path   "CREATE TABLE t (j JSON(a.b UInt32, SKIP x, SKIP REGEXP 'y.*', max_dynamic_paths = 8)) ENGINE = Memory"
emit 63_ttl_group_by      "CREATE TABLE t (a UInt64, k UInt64, d Date) ENGINE = MergeTree ORDER BY (k, a) TTL d + INTERVAL 1 MONTH GROUP BY k SET a = max(a)"
emit 64_projection_index  "ALTER TABLE t ADD PROJECTION p INDEX a TYPE minmax WITH SETTINGS (x = 1)"
emit 65_refresh_mv        "CREATE MATERIALIZED VIEW mv REFRESH EVERY 1 DAY OFFSET 1 HOUR APPEND (a UInt64) ENGINE = Memory AS SELECT 1 AS a"
emit 66_insert_infile     "INSERT INTO t FROM INFILE 'data.csv' COMPRESSION 'gzip' FORMAT CSV"

# --- transaction control ---
emit 67_tcl_begin         "BEGIN TRANSACTION"
emit 68_tcl_commit        "COMMIT"
emit 69_tcl_rollback      "ROLLBACK"
emit 70_tcl_set_snapshot  "SET TRANSACTION SNAPSHOT 42"

# --- workload / resource / named collection DDL (sparse native nodes) ---
emit 71_create_named_collection "CREATE NAMED COLLECTION IF NOT EXISTS nc AS host = 'localhost', port = 9000 OVERRIDABLE, password = 'secret' NOT OVERRIDABLE"
emit 72_create_workload         "CREATE WORKLOAD production IN all SETTINGS max_requests = 100, weight = 5 FOR cpu"
emit 73_create_resource         "CREATE OR REPLACE RESOURCE io (READ DISK fast, WRITE ANY DISK)"
# Named-collection / workload change values are typed `{value_type, value}` (unlike
# a Settings-clause change, whose value is an untyped string). A named collection
# has no schema to recover the type from, so the tag is what keeps the value forms
# from colliding: `i = 5` (UInt64) vs `s = '5'` (String) both stringify to "5",
# `neg = -5` (Int64) vs its string form, `fn = disk(...)` (a function stored as a
# CustomType) vs the same text as a String, and `one = 1.0` (Float64) would read
# back as an integer without the tag. `half = 2.5` is a non-integral float control.
emit 124_named_collection_value_types "CREATE NAMED COLLECTION nc AS i = 5, s = '5', neg = -5, one = 1.0, half = 2.5, fn = disk(type = 'local')"
emit 125_workload_value_types         "CREATE WORKLOAD w SETTINGS max_requests = 100, max_cost = 1.5"

# --- DROP forms of the sparse-node DDL (name + if_exists live in plain members) ---
emit 74_drop_named_collection   "DROP NAMED COLLECTION IF EXISTS nc ON CLUSTER c"
emit 75_drop_workload           "DROP WORKLOAD IF EXISTS production"
emit 76_drop_resource           "DROP RESOURCE io"

# --- SHOW variants: modifiers, ILIKE filter, WHERE, LIMIT, SHOW CLUSTER name ---
emit 77_show_cluster            "SHOW CLUSTER c"
emit 78_show_changed_settings   "SHOW CHANGED SETTINGS ILIKE '%mem%'"
emit 79_show_tables_where_limit "SHOW TABLES FROM db WHERE name != 'x' LIMIT 5"
emit 80_show_merges             "SHOW MERGES"

# --- SYSTEM operands (sparse node): sync-replica mode + source list, drop replica,
#     suspend seconds, filesystem cache key/offset, START LISTEN server type, flush logs ---
emit 81_system_sync_replica     "SYSTEM SYNC REPLICA db.t LIGHTWEIGHT FROM 'r1', 'r2'"
emit 82_system_drop_replica     "SYSTEM DROP REPLICA 'r' FROM ZKPATH '/clickhouse/tables/t'"
emit 83_system_suspend          "SYSTEM SUSPEND FOR 5 SECOND"
emit 84_system_drop_fs_cache    "SYSTEM DROP FILESYSTEM CACHE 'cache1' KEY k OFFSET 10"
emit 85_system_start_listen     "SYSTEM START LISTEN QUERIES ALL EXCEPT MYSQL, TCP"
emit 86_system_flush_logs       "SYSTEM FLUSH LOGS query_log, part_log"

# --- BACKUP / RESTORE: trailing FORMAT, PARTITIONS, AS, EXCEPT TABLES, async setting ---
emit 87_backup_format           "BACKUP TABLE db.t AS db.t2 PARTITIONS 'p1', 'p2' TO Disk('d', 'path') SETTINGS async = 1 FORMAT Null"
emit 88_restore_except_tables   "RESTORE DATABASE db AS db2 EXCEPT TABLES t1 FROM Disk('d', 'p') ASYNC"

# --- DROP MASKING POLICY: target lives in a dedicated struct, not `names` ---
emit 89_drop_masking_policy     "DROP MASKING POLICY IF EXISTS mp ON db.tbl"

# --- SHOW COLUMNS / SHOW SETTING: irreducibly lossy in the native AST — the
#     table, FROM db, LIKE, WHERE, LIMIT, EXTENDED/FULL and the setting name
#     all live in plain members (empty `children`), so the serializer must
#     surface them explicitly or the formatter cannot round-trip the query ---
emit 90_show_columns            "SHOW COLUMNS FROM tab"
emit 91_show_columns_full_like  "SHOW EXTENDED FULL COLUMNS FROM t FROM db LIKE 'a%'"
emit 92_show_columns_where_limit "SHOW COLUMNS FROM t WHERE x = 1 LIMIT 5"
emit 93_show_setting            "SHOW SETTING max_threads"
# SHOW FUNCTIONS drops its LIKE natively; SHOW INDEXES is a distinct class whose
# getID is a mis-set "ShowColumns" — disambiguated to "ShowIndexes" — and drops
# table / FROM db / WHERE / EXTENDED.
emit 94_show_functions_like     "SHOW FUNCTIONS ILIKE 'a%'"
emit 95_show_indexes            "SHOW EXTENDED INDEXES FROM t FROM db WHERE x = 1"

# --- REFRESH strategy: the full clause in both contexts. RANDOMIZE FOR
#     (`spread`), DEPENDS ON (`dependencies`) and refresh-level SETTINGS live in
#     dedicated RefreshStrategy members alongside period/offset — 65 above only
#     exercised EVERY/OFFSET/APPEND, so these pin the rest, incl. the
#     ALTER ... MODIFY REFRESH path (AlterCommand MODIFY_REFRESH). ---
emit 96_refresh_mv_full         "CREATE MATERIALIZED VIEW mv REFRESH EVERY 1 DAY OFFSET 1 HOUR RANDOMIZE FOR 5 MINUTE DEPENDS ON a, b SETTINGS x = 1 APPEND (a UInt64) ENGINE = Memory AS SELECT 1 AS a"
emit 97_alter_modify_refresh    "ALTER TABLE mv MODIFY REFRESH EVERY 1 DAY OFFSET 2 HOUR RANDOMIZE FOR 10 MINUTE DEPENDS ON t1, t2 SETTINGS y = 2"

# --- Projection ORDER BY: stored as a single node (tuple() for multi-key, bare
#     for a lone key), so it must be flattened to a key list. 98 pins the
#     single-key case (bare identifier — previously serialized as []); 99 the
#     multi-key case incl. a function key (previously leaked the tuple wrapper /
#     dropped the function name). ---
emit 98_projection_order_single "ALTER TABLE t ADD PROJECTION p (SELECT a ORDER BY a)"
emit 99_projection_order_multi  "ALTER TABLE t ADD PROJECTION p (SELECT a, b GROUP BY a, b ORDER BY f(a), b)"

# --- output clause (ASTQueryWithOutput): the INTO OUTFILE modifiers, trailing
#     FORMAT, and the settings-after-FORMAT placement (a SETTINGS before FORMAT
#     lands on the operand select instead — see 20 above) ---
emit 100_outfile_format_settings "SELECT 1 INTO OUTFILE '/tmp/f.tsv' TRUNCATE AND STDOUT FORMAT TSV SETTINGS max_threads = 1"
emit 101_outfile_compression     "SELECT 1 INTO OUTFILE '/tmp/f.csv.gz' APPEND COMPRESSION 'gzip' LEVEL 3 FORMAT CSV"

# --- Map field: a map-valued setting. map('k', 1) in 01 is a function call, so
#     this is the only fixture reaching the Map branch of fieldToJSON. ---
emit 102_map_setting             "SELECT 1 SETTINGS additional_table_filters = {'tbl': 'x = 1'}"

# --- TRUNCATE ALL TABLES: has_all / has_tables plus the table-name pattern ---
emit 103_truncate_all_tables     "TRUNCATE ALL TABLES FROM db LIKE '%tmp%'"

# --- CREATE / DROP FUNCTION: ON CLUSTER, and the drop form whose name and
#     if_exists live in plain string members ---
emit 104_create_function_cluster "CREATE FUNCTION f ON CLUSTER c AS x -> x + 1"
emit 105_drop_function           "DROP FUNCTION IF EXISTS f"

# --- ATTACH variants and UNDROP: attach_from_path, uuid, the AS REPLICATED
#     conversion marker, and the UNDROP branch (cluster + uuid fallback) ---
emit 106_attach_from_path        "ATTACH TABLE t FROM '/data/t' (x UInt8) ENGINE = File(TSV)"
emit 107_attach_uuid             "ATTACH TABLE t UUID '123e4567-e89b-12d3-a456-426614174000' (x UInt8) ENGINE = Memory"
emit 108_attach_as_replicated    "ATTACH TABLE t AS REPLICATED"
emit 109_undrop                  "UNDROP TABLE t UUID '123e4567-e89b-12d3-a456-426614174000'"

# --- access management: the statements not covered by 47-58/89 ---
emit 110_check_grant             "CHECK GRANT SELECT(x) ON db.t"
emit 111_set_role                "SET ROLE r1, r2"
emit 112_move_access_entity      "MOVE ROLE r TO local_directory"
emit 113_execute_as              "EXECUTE AS u1 SELECT 1"
emit 114_show_create_user        "SHOW CREATE USER u"
emit 115_show_users              "SHOW USERS"
emit 116_create_masking_policy   "CREATE MASKING POLICY mp ON db.t UPDATE x = 'masked' WHERE x != '' TO r1 PRIORITY 2"

# --- access helpers not reached above: PublicSSHKey, DatabaseOrNone, the
#     name@host split, VALID UNTIL ---
emit 117_user_ssh_key            "CREATE USER u IDENTIFIED WITH ssh_key BY KEY 'AAAA' TYPE 'ssh-rsa'"
emit 118_user_default_database   "CREATE USER u DEFAULT DATABASE db"
emit 119_user_host_valid_until   "CREATE USER u@'%.example.com' IDENTIFIED BY 'p' VALID UNTIL '2030-01-01'"

# --- SHOW FILESYSTEM CACHES: the `caches` flag on ShowTables ---
emit 120_show_fs_caches          "SHOW FILESYSTEM CACHES"

# --- SYSTEM long tail: schema-cache source, WAIT FAILPOINT action, and the
#     stringified fake_time_for_view ---
emit 121_system_schema_cache     "SYSTEM DROP SCHEMA CACHE FOR S3"
emit 122_system_wait_failpoint   "SYSTEM WAIT FAILPOINT fp PAUSE"
emit 123_system_fake_time        "SYSTEM TEST VIEW db.v SET FAKE TIME '2021-01-01 00:00:00'"

# --- the maximal showcase ---
emit 41_showcase "WITH recent AS (SELECT number AS n, number % 3 AS g FROM numbers(100) WHERE number > 1), (SELECT max(n) FROM recent) AS max_n SELECT DISTINCT a.g AS grp, a.n + b.v AS total, arrayMap(e -> e * max_n, groupArray(a.n)) AS scaled, sum(b.v) OVER (PARTITION BY a.g ORDER BY a.n ROWS BETWEEN 1 PRECEDING AND CURRENT ROW) AS running, count(*) OVER w AS cnt, CASE WHEN a.n > 10 THEN 'hi' ELSE 'lo' END AS bucket, CAST(a.n AS Float64) AS nf FROM recent AS a INNER JOIN (SELECT n, n * 2 AS v FROM recent) AS b ON a.n = b.n PREWHERE a.n != 0 WHERE a.n IN (SELECT n FROM recent) AND b.v BETWEEN 1 AND 1000 GROUP BY a.g, a.n WITH ROLLUP HAVING sum(b.v) > 10 WINDOW w AS (PARTITION BY a.g ORDER BY a.n DESC) QUALIFY running >= 0 ORDER BY total DESC NULLS LAST WITH FILL FROM 0 TO 100 STEP 10 INTERPOLATE (nf AS nf + 1) LIMIT 5 BY a.g LIMIT 100 OFFSET 10 SETTINGS max_threads = 4"

echo "Done: $(ls "$OUT"/*.sql | wc -l | tr -d ' ') cases."
