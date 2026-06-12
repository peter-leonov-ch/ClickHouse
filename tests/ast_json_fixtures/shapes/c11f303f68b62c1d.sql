CREATE TABLE test_map_contains_keys
(
    `ResourceAttributes` Map(LowCardinality(String), String),
    INDEX idx_res_attr_value mapKeys(ResourceAttributes) TYPE bloom_filter(0.01) GRANULARITY 1,
)
ORDER BY tuple()
