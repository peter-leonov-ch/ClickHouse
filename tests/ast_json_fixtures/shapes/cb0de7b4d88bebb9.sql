CREATE TABLE IF NOT EXISTS t
(
    id    UUID,
    d     DateTime
)
ENGINE = MergeTree
PARTITION BY toDate(d)
ORDER BY id
