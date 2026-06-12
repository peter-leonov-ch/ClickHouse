CREATE MATERIALIZED VIEW test_view (Rows UInt64,  MaxHitTime DateTime('America/Los_Angeles')) AS SELECT count() AS Rows, max(UTCEventTime) AS MaxHitTime FROM test_table
