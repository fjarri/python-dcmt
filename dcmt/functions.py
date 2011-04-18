import numpy
from time import time
from ctypes import Structure, POINTER, c_int, c_uint32, addressof, sizeof

from _libdcmt import create_mt_structs, fill_mt_structs, \
	init_mt_struct, fill_rand_int, free_mt_structs
from .exceptions import DcmtParameterError


def validate_seed(seed):
	if seed is None:
		return int(time()) % (2 ** 32)
	elif seed < 0 or seed > 2 ** 32 - 1:
		raise DcmtParameterError("Seed must be between 0 and 2^32-1")

	return seed

def validate_parameters(wordlen=32, exponent=521, start_id=0, max_id=0, seed=None):

	if start_id > max_id:
		raise DcmtParameterError("Starting ID must be equal to or lower than maximum ID")

	if start_id < 0 or max_id > 65535:
		raise DcmtParameterError("All IDs must lie between 0 and 65535")

	if wordlen == 31 and exponent == 521 and start_id <= 9 and max_id >= 9:
		raise DcmtParameterError("Known bug: cannot create generator for wordlen=31, exponent=521, id=9")

	if wordlen not in (31, 32):
		raise DcmtParameterError("Word length must be equal to 31 or 32")

	if exponent not in (521, 607, 1279, 2203, 2281, 3217, 4253, 4423,
			9689, 9941, 11213, 19937, 21701, 23209, 44497):
		raise DcmtParameterError("Mersenne exponent is incorrect or not from a supported set")

	return wordlen, exponent, start_id, max_id, validate_seed(seed)

def create_mts(**kwds):

	wordlen, exponent, start_id, max_id, seed = validate_parameters(**kwds)

	# This formula is not exported in dcmt API,
	# so I had to take it from seive.c::init_mt_search()
	state_len = exponent / wordlen + 1

	# Original library allocates two separate pieces of memory on a heap,
	# because the size of mt_struct depends on state vector size.
	# Since I will not use dcmt functions to delete mt_struct instances,
	# it is more convenient to keep everything in one struct,
	# using dynamic features of Python to build necessary class.
	class mt_struct(Structure):
		_fields_ = [
			("aaa", c_uint32),
			("mm", c_int),
			("nn", c_int),
			("rr", c_int),
			("ww", c_int),
			("wmask", c_uint32),
			("umask", c_uint32),
			("lmask", c_uint32),
			("shift0", c_int),
			("shift1", c_int),
			("shiftB", c_int),
			("shiftC", c_int),
			("maskB", c_uint32),
			("maskC", c_uint32),

			# filled on generator initialization
			("i", c_int),
			("state", POINTER(c_uint32)),

			# not present in original mt_struct
			("state_vec", c_uint32 * state_len)
		]

	ptr, count = create_mt_structs(wordlen, exponent, start_id, max_id, seed)

	# being extra-paranoidal
	try:
		mts = (mt_struct * count)()
	except:
		free_mt_structs(ptr, count)
		raise

	# frees original structures inside
	fill_mt_structs(addressof(mts), sizeof(mt_struct), mt_struct.state_vec.offset, ptr, count)

	return mts

def init_mt(mt, seed=None):
	seed = validate_seed(seed)
	init_mt_struct(addressof(mt), seed)

def rand(mt, shape):
	randoms = numpy.empty(shape, dtype=numpy.uint32)
	fill_rand_int(addressof(mt), randoms)
	return randoms
