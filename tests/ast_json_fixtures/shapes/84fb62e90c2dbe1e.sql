ALTER TABLE test_move_partition_throttling MOVE PARTITION tuple() TO VOLUME 'remote' SETTINGS max_remote_write_network_bandwidth=1600000
