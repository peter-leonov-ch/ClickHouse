WITH sub AS
(
  SELECT text
  FROM tab
  WHERE hasAnyTokens(text, ['Alick'])
)
SELECT *
FROM
(
  SELECT text
  FROM tab
  WHERE hasAnyTokens(text, ['Alick'])
)
WHERE (SELECT * FROM sub) != ''
SETTINGS use_skip_indexes_on_data_read = 1
