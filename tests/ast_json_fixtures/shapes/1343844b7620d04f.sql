DESCRIBE ( SELECT '1947 #3 QUERY - FALSE',
                  id,
                  src.value - deltas_sum as delta
            FROM src
            LEFT JOIN
            (
                SELECT id, sum(delta) as deltas_sum FROM dst
                WHERE id IN (SELECT id FROM src WHERE not sleepEachRow(0.001))
                GROUP BY id
            ) _a
            USING (id)
    ) FORMAT Null
