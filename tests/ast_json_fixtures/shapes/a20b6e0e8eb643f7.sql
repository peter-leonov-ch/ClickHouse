insert into function url('http://127.0.0.1/foo.tsv', 'TabSeparated', 'key Int') settings http_max_tries=1, materialized_views_ignore_errors=1 values (2)
