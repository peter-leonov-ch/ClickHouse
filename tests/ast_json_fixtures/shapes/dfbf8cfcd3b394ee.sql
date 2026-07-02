select v, variantElement(v, 'Bool') from format(CSV, 'v Variant(String, Bool)', '\\N\ntruee\ntrue') format CSV
