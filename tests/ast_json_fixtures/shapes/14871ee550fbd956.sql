CREATE TABLE IF NOT EXISTS uuid
(
    created_at DateTime,
    id UUID
)
ENGINE = MergeTree
PARTITION BY toDate(created_at)
ORDER BY (created_at, id)
