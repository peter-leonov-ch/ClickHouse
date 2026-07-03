select v, variantElement(v, 'Bool') from format(Values, 'v Variant(String, Bool)', '(NULL), (true)') format Values
