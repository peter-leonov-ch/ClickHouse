CREATE TABLE url_na_log
(
    `SiteId` UInt32,
    `DateVisit` Date
)
ENGINE = MergeTree
PRIMARY KEY SiteId
ORDER BY (SiteId, DateVisit)
SETTINGS index_granularity_bytes = 1000000, index_granularity = 1000, min_bytes_for_wide_part = 0
