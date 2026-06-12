CREATE TABLE IF NOT EXISTS first engine = MergeTree PARTITION BY (inn, toYYYYMM(received)) ORDER BY (inn, sessionId)
AS SELECT now() AS received, '123456789' AS inn, '42' AS sessionId
