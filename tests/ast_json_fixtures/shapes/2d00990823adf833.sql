SELECT extractAllGroupsHorizontal('hello world', '(\\w+)') SETTINGS regexp_max_matches_per_row = 1000000 FORMAT Null
