CREATE MATERIALIZED VIEW test_view_filtered (EventDate Date, CounterID UInt32) POPULATE AS SELECT CounterID, EventDate FROM test_table WHERE EventDate < '2013-01-01'
