CREATE ROW POLICY IF NOT EXISTS row_policy ON table_with_dot_column USING toDate(date) >= today() - 30 TO ALL
