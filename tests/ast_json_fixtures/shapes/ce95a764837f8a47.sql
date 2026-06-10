with
  ('t' || x) as y,
  'rue' as x
select 1 from tab where y = 'true' settings enable_analyzer=0
