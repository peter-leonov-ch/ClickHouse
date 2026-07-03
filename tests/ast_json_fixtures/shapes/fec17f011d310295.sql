CREATE MATERIALIZED VIEW mview on cluster test_shard_localhost
ENGINE =  SummingMergeTree ORDER BY (day, card_id)
as SELECT
    toDate(timestamp) AS day,
    card_id,
    count(*) AS card_view
FROM source GROUP BY (day, card_id)
