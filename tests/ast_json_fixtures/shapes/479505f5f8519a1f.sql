WITH cte_test_table_for_in AS (SELECT id FROM test_table_for_in) SELECT id, value FROM test_table WHERE id IN cte_test_table_for_in
