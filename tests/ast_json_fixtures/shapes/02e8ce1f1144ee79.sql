CREATE TABLE outer_distributed AS outer
ENGINE = Distributed('test_cluster_two_shards', currentDatabase(), 'outer', intHash64(organization_id))
