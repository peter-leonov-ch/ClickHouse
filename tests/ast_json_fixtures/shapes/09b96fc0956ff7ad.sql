CREATE TABLE t0 ENGINE = IcebergS3(s3_conn, filename = 'issue87414/test/t0') settings iceberg_metadata_file_path = 'metadata/v2.metadata.json'
