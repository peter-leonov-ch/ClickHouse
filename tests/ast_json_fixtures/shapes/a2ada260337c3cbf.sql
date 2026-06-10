select * from test_qualify qualify row_number() over (order by number) = 50 SETTINGS enable_analyzer = 0
