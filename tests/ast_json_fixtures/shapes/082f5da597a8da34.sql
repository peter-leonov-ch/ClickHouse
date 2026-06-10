WITH
    '{"1":{"key":"value"}}' AS data,
    JSONExtract(data, 'Tuple("1" Tuple(key String))') AS parsed_json
SELECT parsed_json AS ssid
