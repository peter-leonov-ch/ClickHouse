select v, variantElement(v, 'Bool') from format(TSV, 'v Variant(String, Bool)', '\\N\ntruee\ntrue') format TSV
