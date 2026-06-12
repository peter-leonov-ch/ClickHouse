CREATE TABLE t_bad_constraint(a UInt32, s String, CONSTRAINT c1 ASSUME a = toUInt32(s)) ENGINE = MergeTree ORDER BY tuple()
