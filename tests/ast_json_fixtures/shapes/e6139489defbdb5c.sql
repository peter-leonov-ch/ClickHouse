SELECT schema_inference_mode, splitByChar('/', source)[-1] as file, schema FROM system.schema_inference_cache WHERE file = '03036_archive1.zip::example1.csv' ORDER BY file
