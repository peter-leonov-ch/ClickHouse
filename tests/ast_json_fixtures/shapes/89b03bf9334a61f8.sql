create materialized view test_mv_00609 uuid '00000609-1000-4000-8000-000000000001' Engine=MergeTree(date, (a), 8192) populate as select a, toDate('2000-01-01') date from test_00609
