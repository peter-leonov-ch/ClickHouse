WITH arrayJoin(`ParsedParams.Key2`) AS pp SELECT ParsedParams.Key2 AS x FROM test.hits WHERE NOT ignore(pp) ORDER BY x ASC LIMIT 2
