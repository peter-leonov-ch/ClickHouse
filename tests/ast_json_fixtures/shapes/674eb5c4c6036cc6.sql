SELECT *, d.* FROM ( SELECT 1 AS id, 2 AS value ) SEMI LEFT JOIN ( SELECT 1 AS id, 3 AS values ) AS d USING id
