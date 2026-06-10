with cte_4 as (select
    rank() over w0 as c_2_c2398_0
  from
    t3 as ref_15
  window w0 as (partition by ref_15.c_2_c16_0 order by ref_15.c_2_c16_0 desc))
select distinct
    ref_39.c_2_c2398_0 as c_9_c2479_0
  from
    cte_4 as ref_39
