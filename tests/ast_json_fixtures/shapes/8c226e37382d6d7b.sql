SELECT (SELECT min(*) FROM (SELECT t0.c0)) AS a0, (SELECT a0) FROM (SELECT 1 c0) AS t0 SETTINGS allow_experimental_correlated_subqueries = 1
