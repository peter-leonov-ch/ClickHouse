with (select randConstant()) as b select b = b, a = b, `a=a` from (with (select randConstant()) as a select a, a = a as `a=a`)
