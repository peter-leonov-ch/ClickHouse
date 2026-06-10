WITH range(0, 8) AS resolutions,
    [(-122.40898669999721, 37.81331899998324), (-122.35447369999936, 37.71980619999785), (-122.4798767000009, 37.815157199999845)] AS ring
SELECT resolution, h3PolygonToCells(ring, arrayJoin(resolutions) AS resolution)
ORDER BY resolution
