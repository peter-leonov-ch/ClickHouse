CREATE TABLE tab
(
    foo Array(LowCardinality(String)),
    INDEX idx foo TYPE bloom_filter
)
ENGINE = MergeTree
PRIMARY KEY tuple()
