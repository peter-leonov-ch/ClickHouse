CREATE TABLE tab1
(
 `valueDate` Date,
 `bb_ticker` String,
 `ric` String,
 `update_timestamp` DateTime,
 INDEX tab1_bb_ticker_idx bb_ticker TYPE bloom_filter GRANULARITY 4,
 INDEX tab1_ric_idx ric TYPE bloom_filter GRANULARITY 4
)
ENGINE = ReplacingMergeTree(update_timestamp)
PRIMARY KEY (valueDate, bb_ticker, ric)
ORDER BY (valueDate, bb_ticker, ric)
SETTINGS index_granularity = 111, index_granularity_bytes = 0, compress_primary_key = 0
