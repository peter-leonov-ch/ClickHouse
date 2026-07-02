select transform(name, ['Henry', 'Irene', 'Dave', 'Cindy'], ['Henry or Irene', 'Henry or Irene', 'Dave or Cindy', 'Dave or Cindy']) AS name, department, salary from (SELECT * FROM employees ORDER BY id, name, department, salary)
order by salary desc
limit 5
format PrettyCompactNoEscapes
