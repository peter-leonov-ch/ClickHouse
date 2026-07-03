SELECT UserID, toMinute(EventTime) AS m, uniq(SearchPhrase) as u, count() as c FROM test.hits_s3 GROUP BY UserID, m, SearchPhrase ORDER BY UserID DESC LIMIT 10 FORMAT Null
