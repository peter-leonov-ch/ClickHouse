CREATE MATERIALIZED VIEW mv2
AS SELECT n1.key as k, n1.value as v1, n2.value as v2 from n1 JOIN n2 ON n1.key = n2.key ORDER BY n1.key
