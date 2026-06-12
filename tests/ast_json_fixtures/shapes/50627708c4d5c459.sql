CREATE TABLE github_events AS gen ENGINE=MergeTree ORDER BY (event_type, repo_name, created_at) SETTINGS index_granularity = 8192, index_granularity_bytes = '10Mi'
