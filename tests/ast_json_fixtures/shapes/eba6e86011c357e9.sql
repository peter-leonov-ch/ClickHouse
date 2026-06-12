select distinct * from dist_01223 where key global in (select toInt32(1))
