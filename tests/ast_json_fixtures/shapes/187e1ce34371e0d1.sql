WITH
    sum(bytes) as s,
    data as (
      SELECT
        formatReadableSize(s),
        table
      FROM another_fake
      GROUP BY table
      ORDER BY s
    )
select * from data
FORMAT Null
