#!/usr/bin/env bash

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

# Snapshot of the complete set of `type` ids (astTypeName outputs) emitted over
# a corpus that touches every enriched class. If a new getID contains a space
# it can silently collide on its trimmed prefix (the Dictionary* case) — this
# guards against that. The set is sorted and de-duplicated for stability.

ast() {
    $CLICKHOUSE_CLIENT --format TSVRaw -q "EXPLAIN AST json = 1 $1" | jq -r '[.. | objects | (.type // empty)] | .[]'
}

{
    ast "WITH cte AS (SELECT 1) SELECT DISTINCT a + b AS s, count() OVER (PARTITION BY g ORDER BY t ROWS BETWEEN 1 PRECEDING AND CURRENT ROW), f(x) IGNORE NULLS, CAST(x AS Int64), [1], (1,2), * EXCEPT (q) REPLACE (z + 1 AS z), COLUMNS('a.*') FROM r ARRAY JOIN arr AS e INNER JOIN s ON r.id = s.id PREWHERE p WHERE w GROUP BY g WITH ROLLUP HAVING h QUALIFY n ORDER BY t WITH FILL INTERPOLATE (y AS y + 1) LIMIT 1, 2 BY g SETTINGS max_threads = 1"
    ast "SELECT 1 INTERSECT SELECT 2"
    ast "SELECT t.* FROM t"
    ast "CREATE TABLE c (a UInt64, b String DEFAULT 'x' CODEC(ZSTD), INDEX i a TYPE minmax GRANULARITY 1, CONSTRAINT ck CHECK a > 0, PROJECTION p (SELECT a)) ENGINE = MergeTree PARTITION BY a ORDER BY a TTL a + INTERVAL 1 DAY SETTINGS index_granularity = 8192"
    ast "CREATE DICTIONARY d (id UInt64) PRIMARY KEY id SOURCE(CLICKHOUSE(TABLE 't')) LAYOUT(HASHED()) LIFETIME(MIN 1 MAX 2) RANGE(MIN s MAX e) SETTINGS(max_threads = 1)"
    ast "CREATE FUNCTION fn AS (x) -> x + 1"
    ast "INSERT INTO t (a) SELECT 1"
    ast "ALTER TABLE t ADD COLUMN x UInt8, MOVE PARTITION 1 TO DISK 'd'"
    ast "ALTER TABLE t UPDATE a = 1 WHERE b"
    ast "DELETE FROM t WHERE x"
    ast "UPDATE t SET a = 1 WHERE b"
    ast "DROP TABLE IF EXISTS t"
    ast "OPTIMIZE TABLE t FINAL"
    ast "RENAME TABLE a TO b"
    ast "KILL QUERY WHERE x SYNC"
    ast "SYSTEM RELOAD DICTIONARY d"
    ast "EXPLAIN SYNTAX SELECT 1"
    ast "DESCRIBE TABLE t"
    ast "SHOW TABLES"
    ast "CHECK TABLE t"
    ast "USE db"
    ast "EXISTS TABLE t"
    ast "SHOW CREATE TABLE t"
    ast "CREATE INDEX i ON t (a) TYPE minmax GRANULARITY 1"
    ast "SELECT 1 SETTINGS max_threads = 1"
    ast "SELECT * FROM t SAMPLE 1/2"
    ast "GRANT SELECT(x) ON db.t TO u WITH GRANT OPTION"
    ast "REVOKE INSERT ON db.* FROM ALL EXCEPT u"
    ast "CHECK GRANT SELECT ON db.t"
    ast "CREATE USER u IDENTIFIED WITH ssh_key BY KEY 'AAAA' TYPE 'ssh-rsa' HOST IP '127.0.0.1' DEFAULT ROLE r SETTINGS max_threads = 1 DEFAULT DATABASE NONE GRANTEES ANY"
    ast "ALTER USER u DROP SETTINGS max_threads"
    ast "CREATE ROLE r SETTINGS PROFILE p"
    ast "CREATE QUOTA q KEYED BY user_name FOR INTERVAL 1 HOUR MAX queries = 1 TO r"
    ast "SET DEFAULT ROLE r TO u"
    ast "CREATE ROW POLICY p ON db.t FOR SELECT USING x > 0 TO r"
    ast "CREATE SETTINGS PROFILE sp SETTINGS max_threads = 1 TO ALL"
    ast "CREATE MASKING POLICY m ON db.t UPDATE x = mask(x) TO r"
    ast "DROP ROW POLICY p ON db.t"
    ast "MOVE USER u TO storage"
    ast "SHOW GRANTS FOR u"
    ast "SHOW CREATE USER u"
    ast "SHOW QUOTAS"
    ast "EXECUTE AS u"
    ast "CREATE TABLE en (e Enum8('a' = 1, 'b' = 2), tp Tuple(x UInt8, y String), s String COLLATE binary, j JSON(a.b UInt32, SKIP p)) ENGINE = Memory"
    ast "CREATE MATERIALIZED VIEW mv REFRESH EVERY 1 DAY (a UInt64) ENGINE = Memory AS SELECT 1 AS a"
    ast "COMMIT"
} | LC_ALL=C sort -u | grep -vxE 'Unbounded|Current|Offset'
# (Unbounded/Current/Offset are the frame_begin/frame_end `type` discriminator
#  values, not node type ids — excluded so this snapshots only astTypeName output.)
