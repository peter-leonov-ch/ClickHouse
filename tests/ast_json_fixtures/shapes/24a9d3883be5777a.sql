select distinct sum(number) over w as x, max(number) over w as y from remote('127.0.0.{1,2}', '', t_01568) window w as (partition by p) order by x, y
