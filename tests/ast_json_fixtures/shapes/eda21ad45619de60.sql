CREATE TABLE foo (key int, INDEX i1 key TYPE minmax GRANULARITY 1) Engine=MergeTree() ORDER BY key
