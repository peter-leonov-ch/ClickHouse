ALTER TABLE restore_01640 ATTACH PARTITION tuple(toYYYYMM(toDate('2021-01-01'))) SETTINGS insert_keeper_fault_injection_probability=0
