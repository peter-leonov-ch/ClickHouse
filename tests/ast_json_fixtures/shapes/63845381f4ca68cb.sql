SELECT '(forward, head) id < 10', id, sequenceNextNode('forward', 'head')(dt, action, 1) AS next_node FROM test_sequenceNextNode WHERE id < 10 GROUP BY id ORDER BY id
