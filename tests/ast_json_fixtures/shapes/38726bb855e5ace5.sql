select count(), * from dist_01247 where number = _shard_num-1 group by number order by number limit 1 offset 1
