SELECT isNull(number)::Boolean, now() FROM numbers(2) GROUP BY isNull(number)::Boolean, now() FORMAT Null
