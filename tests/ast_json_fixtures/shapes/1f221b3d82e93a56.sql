with (select groupArray(id) from bbb) as ids
select *
  from aaa
 where has(ids, id)
order by id
