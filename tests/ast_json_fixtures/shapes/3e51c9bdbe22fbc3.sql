CREATE TABLE t_detach_attach_patches_dst AS t_detach_attach_patches
ENGINE = ReplicatedMergeTree('/zookeeper/{database}/t_lwu_on_fly_dst/', '1')
ORDER BY a PARTITION BY id
SETTINGS
    enable_block_number_column = 1,
    enable_block_offset_column = 1
