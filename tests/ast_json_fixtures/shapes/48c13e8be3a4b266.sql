INSERT INTO TABLE FUNCTION s3('http://localhost:9001/foo/{_partition_id}', 'admin', 'admin', 'CSV', 'id Int32, val String') PARTITION BY val VALUES (1, '')
