SELECT x, toTypeName(x) FROM format('JSONEachRow', '{"x" : 42}, {"x" : "Hello"}') FORMAT Pretty
