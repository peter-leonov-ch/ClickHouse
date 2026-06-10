SELECT left.x, (right.x IS NULL)::Boolean FROM left LEFT OUTER JOIN right ON left.x = right.x GROUP BY ALL
