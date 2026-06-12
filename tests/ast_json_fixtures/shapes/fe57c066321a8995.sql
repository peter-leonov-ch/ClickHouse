create table trunc (n int, primary key n) partition by n % 10
