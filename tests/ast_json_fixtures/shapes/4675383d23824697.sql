CREATE TABLE demo_loan_01568_dist AS shard_0.demo_loan_01568 ENGINE=Distributed('test_cluster_two_shards_different_databases', '', 'demo_loan_01568', id % 2)
