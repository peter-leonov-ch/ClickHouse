select sum(number) as s from remote('127.0.0.{1,2}', numbers(10)) where (intDiv(number, 2) as key) != 1 group by key order by key with fill interpolate (s as 100500)
