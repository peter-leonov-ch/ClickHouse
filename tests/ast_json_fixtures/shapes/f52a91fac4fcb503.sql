SELECT '100' AS number FROM d_numbers AS n WHERE n.number = 100 LIMIT 2 SETTINGS max_threads = 1, prefer_localhost_replica=1
