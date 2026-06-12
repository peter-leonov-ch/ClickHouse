SELECT CounterID, avg(length(URL)) AS l, count() AS c FROM test.hits_s3 WHERE URL != '' GROUP BY CounterID HAVING c > 100000 ORDER BY l DESC LIMIT 25
