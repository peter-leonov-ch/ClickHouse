CREATE TABLE log(
    collectorReceiptTime DateTime,
    eventId String,
    ruleId String,
    PROJECTION ailog_rule_count (
    SELECT
        collectorReceiptTime,
        ruleId,
        count(ruleId)
    GROUP BY
        collectorReceiptTime,
        ruleId
    )
)
ENGINE = MergeTree
PARTITION BY toYYYYMMDD(collectorReceiptTime)
ORDER BY (collectorReceiptTime, eventId)
