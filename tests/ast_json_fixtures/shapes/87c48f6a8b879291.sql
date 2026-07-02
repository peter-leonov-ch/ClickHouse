SELECT arrayJoin([SearchEngineID]) AS search_engine, URL FROM test.hits WHERE SearchEngineID != 0 AND search_engine != 0 FORMAT Null
