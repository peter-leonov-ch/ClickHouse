SELECT toString(d), avg(a) FROM pk_order GROUP BY toString(d) ORDER BY toString(d) LIMIT 5
