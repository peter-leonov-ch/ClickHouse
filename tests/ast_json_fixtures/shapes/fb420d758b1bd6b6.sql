WITH
  users AS (
    WITH t as (
      SELECT * FROM users
    )
    SELECT * FROM t
  )
SELECT *
FROM users
FORMAT Null
