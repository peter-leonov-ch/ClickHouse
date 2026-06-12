#!/usr/bin/env bash

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

# A runnable showcase of `EXPLAIN AST json = 1`: one deeply nested query that
# exercises most of the named slots and node types at once, dumped in full
# (no node filtering). The query only needs to parse, not run.
#
# Clauses / constructs covered:
#   - WITH: a named CTE and a scalar subquery binding
#   - DISTINCT select with aliases
#   - lambda (arrayMap), window function with inline definition and named
#     window, COUNT(*), CASE, CAST
#   - FROM with an INNER JOIN over a subquery, PREWHERE, WHERE with IN
#     (subquery) and BETWEEN
#   - GROUP BY ... WITH ROLLUP, HAVING, WINDOW, QUALIFY
#   - ORDER BY ... DESC NULLS LAST WITH FILL ... INTERPOLATE
#   - LIMIT BY, LIMIT ... OFFSET, SETTINGS

read -r -d '' QUERY <<'SQL'
WITH
    recent AS (SELECT number AS n, number % 3 AS g FROM numbers(100) WHERE number > 1),
    (SELECT max(n) FROM recent) AS max_n
SELECT DISTINCT
    a.g AS grp,
    a.n + b.v AS total,
    arrayMap(e -> e * max_n, groupArray(a.n)) AS scaled,
    sum(b.v) OVER (PARTITION BY a.g ORDER BY a.n ROWS BETWEEN 1 PRECEDING AND CURRENT ROW) AS running,
    count(*) OVER w AS cnt,
    CASE WHEN a.n > 10 THEN 'hi' ELSE 'lo' END AS bucket,
    CAST(a.n AS Float64) AS nf
FROM recent AS a
INNER JOIN (SELECT n, n * 2 AS v FROM recent) AS b ON a.n = b.n
PREWHERE a.n != 0
WHERE a.n IN (SELECT n FROM recent) AND b.v BETWEEN 1 AND 1000
GROUP BY a.g, a.n WITH ROLLUP
HAVING sum(b.v) > 10
WINDOW w AS (PARTITION BY a.g ORDER BY a.n DESC)
QUALIFY running >= 0
ORDER BY total DESC NULLS LAST WITH FILL FROM 0 TO 100 STEP 10 INTERPOLATE (nf AS nf + 1)
LIMIT 5 BY a.g
LIMIT 100 OFFSET 10
SETTINGS max_threads = 4, max_block_size = 1000
SQL

$CLICKHOUSE_CLIENT --format TSVRaw -q "EXPLAIN AST json = 1 $QUERY"
