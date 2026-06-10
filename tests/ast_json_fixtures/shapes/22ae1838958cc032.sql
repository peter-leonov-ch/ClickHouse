CREATE TABLE foo (id UInt64, key AggregateFunction(max, UInt64)) ENGINE MergeTree PARTITION BY key
