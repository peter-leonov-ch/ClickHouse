with 5 as q1, x as (select number+100 as b, number as a from numbers(10) where number > q1) select * from x
