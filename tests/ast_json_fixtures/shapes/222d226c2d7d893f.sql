SELECT AdvEngineID, count() FROM test.hits_s3 WHERE AdvEngineID != 0 GROUP BY AdvEngineID ORDER BY AdvEngineID DESC
