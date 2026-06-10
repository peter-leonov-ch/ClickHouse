INSERT INTO FUNCTION
   s3(
       s3_conn,
       filename = currentDatabase() || '/{_partition_id}/test.parquet',
       format = Parquet
    ) PARTITION BY 2 SELECT
    *
FROM system.numbers
LIMIT 10
