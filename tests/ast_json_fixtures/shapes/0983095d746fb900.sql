SELECT arrayJoin(arrayJoin(arr.k1[])) AS k1 FROM t_json_array ORDER BY toString(k1) FORMAT JSONEachRow
