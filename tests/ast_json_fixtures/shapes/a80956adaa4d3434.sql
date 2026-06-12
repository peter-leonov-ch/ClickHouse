SELECT count(NULL) FROM remote('127.0.0.{1,2}', numbers(3)) GROUP BY number % 2 WITH TOTALS
