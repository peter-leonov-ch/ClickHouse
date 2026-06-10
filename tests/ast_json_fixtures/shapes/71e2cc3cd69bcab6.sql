INSERT INTO bool_test (value,f) FORMAT JSONEachRow {"value":false,"f":"test"}{"value":true,"f":"test"}{"value":0,"f":"test"}{"value":1,"f":"test"}

SELECT value,f FROM bool_test
