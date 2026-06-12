with ('t' || x) as y 
  select 1 from tab where y = 'true' settings enable_analyzer=0
