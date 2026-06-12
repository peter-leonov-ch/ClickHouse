create table dist_in as ephemeral engine=Distributed(test_shard_localhost, currentDatabase(), ephemeral, key) settings background_insert_batch=1
