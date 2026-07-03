select concat('name_', toString(number)) as name, number from numbers(3) format JSONObjectEachRow
