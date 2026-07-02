select query, query_kind, exception_code,
    read_rows, written_rows,
    ProfileEvents['QueriesWithSubqueries'] as QueriesWithSubqueries,
    ProfileEvents['SelectQueriesWithSubqueries'] as SelectQueriesWithSubqueries,
    ProfileEvents['AsyncInsertRows'] as AsyncInsertRows,
    ProfileEvents['SelfDuplicatedAsyncInserts'] as SelfDuplicatedAsyncInserts,
    ProfileEvents['DuplicatedAsyncInserts'] as DuplicatedAsyncInserts
from system.query_log
where
    has(databases, currentDatabase())
    and has(tables, currentDatabase() || '.src_table')
    and type != 'QueryStart'
    and query_kind = 'AsyncInsertFlush'
order by all desc FORMAT Vertical
