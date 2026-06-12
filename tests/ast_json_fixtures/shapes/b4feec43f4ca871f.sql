SELECT domainWithoutWWW(Referer) AS key, avg(length(Referer)) AS l, count() AS c, max(Referer) FROM test.hits_s3 WHERE Referer != '' GROUP BY key HAVING c > 100000 ORDER BY l DESC LIMIT 25
