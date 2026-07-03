CREATE ROW POLICY test_filter_policy_2 ON test_table USING (n % 5) >= 3 TO default
