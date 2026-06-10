SELECT DISTINCT URL, URLDomain, position('http://yandex.ru/', domain(URL)) AS x FROM test.hits WHERE x > 8 ORDER BY position('http://yandex.ru/', domain(URL)) DESC, URL LIMIT 3
