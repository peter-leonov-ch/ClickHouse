CREATE TABLE tab
(
  id UInt32,
  map1 Map(String, String),
  map2 Map(String, String),
  INDEX idx_map1_key mapKeys(map1) TYPE bloom_filter(0.01) GRANULARITY 1,
  INDEX idx_map2_key mapKeys(map2) TYPE bloom_filter(0.01) GRANULARITY 1
)
ENGINE = MergeTree
PRIMARY KEY(id)
