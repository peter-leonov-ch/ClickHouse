select queryID() as t from numbers(10) with totals having t = initialQueryID()
