CREATE TABLE 03443_data
(
    id Int32,
    name String,
    INDEX idx_name name TYPE ngrambf_v1(1, 1024, 3, 0) GRANULARITY 1
)
ENGINE = MergeTree ORDER BY id SETTINGS index_granularity = 1
AS
SELECT 1, 'John' UNION ALL
SELECT 2, 'Ksenia' UNION ALL
SELECT 3, 'Alice'
