select number % 2 as number, count() from numbers(10) where number != 0 group by number % 2 as number
