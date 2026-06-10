ALTER TABLE restore_01640 FETCH PARTITION tuple(toYYYYMM(toDate('2021-01-01')))
  FROM '/clickhouse/{database}/{shard}/tables/test_01640' SETTINGS insert_keeper_fault_injection_probability=0
