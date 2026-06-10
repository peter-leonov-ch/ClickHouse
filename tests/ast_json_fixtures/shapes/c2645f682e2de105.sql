CREATE TABLE numbers2 ORDER BY intHash32(number) SAMPLE BY intHash32(number) AS SELECT number FROM numbers(10)
