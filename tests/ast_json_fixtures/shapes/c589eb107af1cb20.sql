SELECT number, toString(number), range(number), toDate('2000-01-01') + number, toDateTime('2000-01-01 00:00:00') + number FROM system.numbers LIMIT 10 FORMAT CSV
