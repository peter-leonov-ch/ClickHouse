ALTER TABLE 02577_keepermap_delete_update ON CLUSTER test_shard_localhost UPDATE value2 = value2 * 10 + 2 WHERE value2 < 100
