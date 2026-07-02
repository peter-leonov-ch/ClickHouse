select number, concat('name_', toString(number)) as name from numbers(3) format JSONObjectEachRow
