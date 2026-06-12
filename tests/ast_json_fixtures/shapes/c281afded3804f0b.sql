WITH
    (
        SELECT query_id
        FROM system.query_log
        WHERE current_database = currentDatabase() AND Settings['log_processors_profiles']='1'
    ) AS query_id_
SELECT
    name,
    multiIf(
        
        
        
        
        name = 'ExpressionTransform', elapsed_us >= 0.9e6 ? 1 : elapsed_us,
        
        
        name = 'SourceFromSingleChunk', output_wait_elapsed_us >= 0.9e6 ? 1 : output_wait_elapsed_us,
        
        
        input_wait_elapsed_us>=1e6 ? 1 : input_wait_elapsed_us)
    elapsed,
    input_rows,
    input_bytes,
    output_rows,
    output_bytes
FROM system.processors_profile_log
WHERE query_id = query_id_
ORDER BY name
