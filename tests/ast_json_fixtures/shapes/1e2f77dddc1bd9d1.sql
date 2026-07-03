SELECT
    topKResampleState(1048576, 257, 65536, 10)(toString(number), number)
FROM numbers(3)
FORMAT Parquet
