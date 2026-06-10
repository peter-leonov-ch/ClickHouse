CREATE TABLE sample_merge_00314 AS sample_00314_1 ENGINE = Merge(currentDatabase(), '^sample_00314_\\d$')
