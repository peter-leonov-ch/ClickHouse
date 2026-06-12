select number as ad, number, number as pd from numbers(5) order by number*10 with fill interpolate (pd as pd+100, ad as ad+1000)
