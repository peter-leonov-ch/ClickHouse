WITH a AS (select (select 1 WHERE 0) as b)
select 1
from system.one
cross join a
where a.b = 0
