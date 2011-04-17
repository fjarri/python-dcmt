import numpy
from _libdcmt import c_get_randoms

def get_randoms(mts, shape, dtype=numpy.uint32):
	randoms = numpy.empty(shape, dtype=dtype)
	c_get_randoms(mts, randoms)
	return randoms