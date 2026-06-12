SELECT uniq(SearchPhrase), count() AS c FROM test.hits_s3 WHERE SearchPhrase != '' GROUP BY SearchPhrase ORDER BY c DESC LIMIT 10
