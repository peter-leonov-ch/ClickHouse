with (select count() from (select 1 union distinct select 2 except select 1)) as max
select count() from (select 1 union all select max) limit 100
