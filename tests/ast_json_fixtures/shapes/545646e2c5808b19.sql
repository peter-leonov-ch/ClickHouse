with ('t' || x) as y select * from 
  (select 1 from tab where y = 'true') settings enable_analyzer=0
