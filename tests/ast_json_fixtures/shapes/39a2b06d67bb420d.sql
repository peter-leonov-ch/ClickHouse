WITH avg(a) OVER () AS a SELECT a, id FROM test SETTINGS allow_experimental_window_functions = 1
