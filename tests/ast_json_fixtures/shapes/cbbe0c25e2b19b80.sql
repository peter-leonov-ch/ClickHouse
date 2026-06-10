WITH
    1734955380 AS start, 1734955680 AS end, 15 AS step, 0 AS staleness,
    timeSeriesRange(start, end, step) as grid
SELECT arrayZip(grid, timeSeriesResampleToGridWithStaleness(start, end, step, staleness)(timestamp, value)) FROM ts_raw_data
