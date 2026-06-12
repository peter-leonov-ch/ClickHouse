SELECT
    ProfileEvents['MutationSomePartColumns'],
    ProfileEvents['MutatedUncompressedBytes'] 
FROM system.part_log WHERE database = currentDatabase() AND table = 't_apply_patches' AND event_type = 'MutatePart'
ORDER BY ALL
