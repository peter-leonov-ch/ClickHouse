CREATE TEMPORARY TABLE table_keeper_map_02525
(
    key String,
    value UInt32
) Engine=KeeperMap('/' || currentDatabase() || '/test02525')
PRIMARY KEY(key)
