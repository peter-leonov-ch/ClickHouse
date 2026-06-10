select n = n_,
	number as n,
	bitOr(bitShiftLeft(bitOr(bitShiftLeft(bitOr(bitShiftLeft(bitOr(bitShiftLeft(bitOr(bitShiftLeft(bitOr(bitShiftLeft(bitOr(bitShiftLeft(b7, 1), b6), 1), b5), 1), b4), 1), b3), 1), b2), 1), b1), 1), b0) as n_,
	bitTest(n, 7) as b7,
	bitTest(n, 6) as b6,
	bitTest(n, 5) as b5,
	bitTest(n, 4) as b4,
	bitTest(n, 3) as b3,
	bitTest(n, 2) as b2,
	bitTest(n, 1) as b1,
	bitTest(n, 0) as b0
from system.numbers limit 256
