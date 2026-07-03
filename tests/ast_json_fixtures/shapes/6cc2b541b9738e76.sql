SELECT k, groupArrayMovingAvg(v) FROM (SELECT * FROM moving_sum_num ORDER BY k, dt) GROUP BY k ORDER BY k FORMAT TabSeparatedWithNamesAndTypes
