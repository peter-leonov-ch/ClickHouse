select x from file(toNullable(concat(currentDatabase(), '/03727_prewhere_intermediate_columns.parquet'))) prewhere x order by x
