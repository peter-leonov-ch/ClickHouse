WITH
    1734955380 AS start, 1734955680 AS end, 15 AS step, 300 AS window, 60 as predict_offset,
    range(start, end + 1, step) as grid
SELECT
    arrayZip(grid, timeSeriesChangesToGrid(start, end, step, window)(toUnixTimestamp(timestamp), value)) as changes_5m,
    arrayZip(grid, timeSeriesResetsToGrid(start, end, step, window)(toUnixTimestamp(timestamp), value)) as resets_5m
FROM ts_raw_data FORMAT Vertical
