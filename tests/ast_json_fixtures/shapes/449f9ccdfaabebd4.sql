select table, sum(primary_key_size) from system.parts where database = currentDatabase() AND table = 'dist_idx_pk_size' group by 1
