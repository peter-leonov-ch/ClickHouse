select groupArray(groupArray(number)) over (rows unbounded preceding) as x from remote('127.0.0.{1,2}', '', t_01568) group by mod(number, 3) order by x settings distributed_group_by_no_merge=1
