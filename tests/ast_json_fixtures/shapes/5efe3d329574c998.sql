SELECT DISTINCT * WHERE 2 <=> materialize(2)
GROUP BY 1
WITH TOTALS QUALIFY ((2 <=> 2) / ((2 IS NOT NULL) IS NULL), *, *, materialize(toNullable(2)), materialize(2), materialize(materialize(2)), 2)
<=> ((2 = *) * 2, *, *, 2, toNullable(2), 2, 2)
