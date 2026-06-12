WITH with_table as (
    SELECT p.a_id, p.b_id, p.c_id FROM parent p
    LEFT JOIN join_table_1 jt1 ON jt1.a_id = p.a_id AND jt1.b_id = p.b_id
    LEFT JOIN join_table_2 jt2 ON jt2.c_id = p.c_id
    WHERE
        p.a_id = 0 AND (jt2.c_id = 0 OR p.created_at = 0)
)
SELECT p.a_id, p.b_id, COUNT(*) as f_count FROM with_table
GROUP BY p.a_id, p.b_id
