select '{"a" : [{"b" : 42}]}'::JSON(a Array(JSON)) as json, json.a.b, json::JSON as json2, dynamicType(json2.a), json2.a[].b
