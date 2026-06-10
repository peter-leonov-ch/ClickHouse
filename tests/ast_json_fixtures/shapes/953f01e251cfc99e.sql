INSERT INTO FUNCTION s3(s3_conn, filename='t_03363_parquet', format=Parquet, partition_strategy='hive') PARTITION BY (year, country) SELECT 2020 as year, 'Brazil' as country
