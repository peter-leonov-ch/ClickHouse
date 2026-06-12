WITH RECURSIVE search_graph AS (
	SELECT *, false AS is_cycle, [tuple(g.f, g.t)] AS path FROM graph g
	UNION ALL
	SELECT g.*, has(path, tuple(g.f, g.t)), arrayConcat(sg.path, [tuple(g.f, g.t)])
	FROM graph g, search_graph sg
	WHERE g.f = sg.t AND NOT is_cycle
)
SELECT * FROM search_graph
SETTINGS query_plan_join_swap_table = 'false'
