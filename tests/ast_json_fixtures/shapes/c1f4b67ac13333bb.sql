select length(splitByChar('*', data_path)), replaceRegexpOne(data_path, '^.*/([^/]*)/' , '\\1') from system.distribution_queue where database = currentDatabase() and table = 'dist_01555' format CSV
