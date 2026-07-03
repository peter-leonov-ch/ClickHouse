SELECT * FROM left_joined_view
WHERE t1_a < 2000
SETTINGS log_comment = 'left_join_view'
FORMAT Null
