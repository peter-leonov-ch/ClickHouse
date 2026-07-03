ALTER TABLE tbl ADD INDEX idx (id+1, id, id+1) TYPE minmax SETTINGS allow_suspicious_indices = 1
