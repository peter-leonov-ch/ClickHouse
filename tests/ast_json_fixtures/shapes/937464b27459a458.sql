select null, isConstant(null), * from (select 2 x) a left join (select null, 3 y) b on y = x
