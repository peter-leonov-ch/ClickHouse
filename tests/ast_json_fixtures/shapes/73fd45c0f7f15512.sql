INSERT INTO t_json_10 FORMAT JSONAsObject {"a": {"b": 1, "c": [{"d": 10, "e": [31]}, {"d": 20, "e": [63, 127]}]}} {"a": {"b": 2, "c": []}}

INSERT INTO t_json_10 FORMAT JSONAsObject {"a": {"b": 3, "c": [{"f": 20, "e": [32]}, {"f": 30, "e": [64, 128]}]}} {"a": {"b": 4, "c": []}}

SELECT DISTINCT arrayJoin(JSONAllPathsWithTypes(o)) as path FROM t_json_10 order by path
