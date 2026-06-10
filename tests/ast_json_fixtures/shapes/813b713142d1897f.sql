SELECT count() = 2 AS assert_exists FROM system.events WHERE name IN ('FilterTransformPassedRows', 'FilterTransformPassedBytes') HAVING assert_exists
