select dim1, dim2, count() from test group by grouping sets ((dim1, dim2), dim1) order by dim1, dim2, count()
