SELECT SearchPhrase FROM test.hits_s3 WHERE SearchPhrase != '' ORDER BY EventTime LIMIT 10 FORMAT Null
