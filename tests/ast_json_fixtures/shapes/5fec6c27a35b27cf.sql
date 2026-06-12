CREATE TABLE mt (a UInt64, b Nullable(String), PRIMARY KEY (a, coalesce(b, 'test')), INDEX b_index b TYPE set(123) GRANULARITY 1)
