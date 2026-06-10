INSERT INTO FUNCTION s3(s3_conn, filename='half_baked', format=Parquet, partition_strategy='hive') PARTITION BY year SELECT 1 AS key, 2020 AS year
