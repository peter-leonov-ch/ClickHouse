CREATE TABLE products as prod_hist ENGINE = Merge(currentDatabase(), '^products_')
