select count() cnt, * from dist_01247 group by number having cnt == 1 limit 1
