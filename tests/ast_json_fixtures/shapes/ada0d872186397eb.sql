CREATE TABLE 03408_local (id Int32, val String) ENGINE = MergeTree ORDER BY tuple() SETTINGS min_bytes_for_wide_part=1
AS
SELECT number % 10, leftPad(toString(number), 2, '0') FROM numbers(50)
