SELECT number, grouping(number % 2, number) RESPECT NULLS AS gr FROM numbers(10) GROUP BY GROUPING SETS ((number), (number % 2)) SETTINGS force_grouping_standard_compatibility = 0
