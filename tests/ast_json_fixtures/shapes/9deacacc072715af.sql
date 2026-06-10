SELECT a, b FROM t_virtual_row_sparse_pk PREWHERE a < 100000 ORDER BY (a, b) LIMIT 5 SETTINGS read_in_order_use_virtual_row = 1
