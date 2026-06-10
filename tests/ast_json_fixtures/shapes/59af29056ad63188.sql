CREATE TABLE numbers1 ENGINE = MergeTree ORDER BY intHash32(number) SAMPLE BY intHash32(number) AS SELECT number FROM numbers(1000)
