INSERT INTO bool_test (value,f) FORMAT CSV On,test

INSERT INTO bool_test (value,f) FORMAT TSV Off	test

SELECT value,f FROM bool_test order by value FORMAT CSV
