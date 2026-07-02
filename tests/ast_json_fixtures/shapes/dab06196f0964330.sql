ALTER TABLE tbl ADD COLUMN `id2` UInt32, MODIFY ORDER BY (id1, id2, id2) SETTINGS allow_suspicious_indices = 1
