WITH
    lineWithInlines AS
    (
        SELECT DISTINCT addressToLineWithInlines(arrayJoin(trace)) AS lineWithInlines FROM system.trace_log WHERE query_id =
        (
            SELECT query_id FROM system.query_log WHERE current_database = currentDatabase() AND log_comment='02161_test_case' ORDER BY event_time DESC LIMIT 1
        )
    )
SELECT 'has inlines:', or(max(length(lineWithInlines)) > 1, max(locate(lineWithInlines[1], ':')) = 0) FROM lineWithInlines SETTINGS short_circuit_function_evaluation='enable'
