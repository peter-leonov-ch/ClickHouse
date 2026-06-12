CREATE TABLE inner_distributed AS inner
ENGINE = Distributed('test_cluster_two_shards', currentDatabase(), 'inner', intHash64(organization_id))
