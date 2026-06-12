WITH range(0, 8) AS resolutions
SELECT DISTINCT resolution, h3PolygonToCells(ring, arrayJoin(resolutions) AS resolution) FROM rings
ORDER BY resolution
