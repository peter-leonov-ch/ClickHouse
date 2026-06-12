WITH
  t as (SELECT sum(number) as x FROM numbers(10))
SELECT t.*
