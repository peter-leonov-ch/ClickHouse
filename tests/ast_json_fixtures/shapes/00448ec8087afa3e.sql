select arrayJoin(val) as nameGroup6 from t1_00729 prewhere notEmpty(toString(nameGroup6)) group by nameGroup6 order by nameGroup6
