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
emit 19_limits            "SELECT x FROM t ORDER BY x LIMIT 5 BY g LIMIT 100 OFFSET 10"
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
emit 37_apply_transform   "SELECT * APPLY(sum) FROM t"
emit 38_replace_transform "SELECT * REPLACE (x + 1 AS y) FROM t"
emit 39_columns_regexp    "SELECT COLUMNS('a.*') FROM t"
emit 40_columns_list      "SELECT COLUMNS(a, b) FROM t"

# --- the maximal showcase ---
emit 41_showcase "WITH recent AS (SELECT number AS n, number % 3 AS g FROM numbers(100) WHERE number > 1), (SELECT max(n) FROM recent) AS max_n SELECT DISTINCT a.g AS grp, a.n + b.v AS total, arrayMap(e -> e * max_n, groupArray(a.n)) AS scaled, sum(b.v) OVER (PARTITION BY a.g ORDER BY a.n ROWS BETWEEN 1 PRECEDING AND CURRENT ROW) AS running, count(*) OVER w AS cnt, CASE WHEN a.n > 10 THEN 'hi' ELSE 'lo' END AS bucket, CAST(a.n AS Float64) AS nf FROM recent AS a INNER JOIN (SELECT n, n * 2 AS v FROM recent) AS b ON a.n = b.n PREWHERE a.n != 0 WHERE a.n IN (SELECT n FROM recent) AND b.v BETWEEN 1 AND 1000 GROUP BY a.g, a.n WITH ROLLUP HAVING sum(b.v) > 10 WINDOW w AS (PARTITION BY a.g ORDER BY a.n DESC) QUALIFY running >= 0 ORDER BY total DESC NULLS LAST WITH FILL FROM 0 TO 100 STEP 10 INTERPOLATE (nf AS nf + 1) LIMIT 5 BY a.g LIMIT 100 OFFSET 10 SETTINGS max_threads = 4"

echo "Done: $(ls "$OUT"/*.sql | wc -l | tr -d ' ') cases."
