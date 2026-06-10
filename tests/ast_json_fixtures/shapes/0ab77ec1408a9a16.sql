CREATE TABLE test__fuzz_2_dist AS test__fuzz_2_local
ENGINE = Distributed(test_cluster_two_shards, currentDatabase(), test__fuzz_2_local, rand())
