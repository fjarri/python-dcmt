import numpy
from time import time
from ctypes import Structure, POINTER, c_int, c_uint32, addressof, sizeof

from _libdcmt import create_mt_structs, fill_mt_structs, \
	fill_mt_structs_stripped, init_mt_struct, fill_rand_int, free_mt_structs
from .exceptions import DcmtParameterError
from .structures import mt_common, mt_stripped, mt_struct_base


def validate_seed(seed):
	"""Return valid seed or raise an exception"""

	if seed is None:
		return int(time()) % (2 ** 32)
	elif seed < 0 or seed > 2 ** 32 - 1:
		raise DcmtParameterError("Seed must be between 0 and 2^32-1")

	return seed

def validate_parameters(wordlen=32, exponent=521, start_id=0, max_id=0, seed=None):
	"""Return valid parameter or raise an exception"""

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
	"""Create array of ctypes Structures with RNG parameters"""

	wordlen, exponent, start_id, max_id, seed = validate_parameters(**kwds)

	ptr, count, state_len = create_mt_structs(wordlen, exponent, start_id, max_id, seed)

	# being extra-paranoidal
	try:
		# extending the ending array in mt_struct_base keeping its name and type
		fields = mt_struct_base._fields_[:]
		fields[-1] = (fields[-1][0], fields[-1][1] * state_len)
		class mt_struct(Structure):
			_fields_ = fields

		mts = (mt_struct * count)()
		fill_mt_structs(addressof(mts), sizeof(mt_struct), mt_struct.state_vec.offset, ptr, count)
	finally:
		free_mt_structs(ptr, count)

	return mts

def init_mt(mt, seed=None):
	"""Initialize RNG structure with given seed"""
	seed = validate_seed(seed)
	init_mt_struct(addressof(mt), seed)

def rand(mt, shape):
	"""Return numpy array with random numbers"""
	randoms = numpy.empty(shape, dtype=numpy.uint32)
	fill_rand_int(addressof(mt), randoms)
	return randoms

def create_mts_stripped(**kwds):
	"""Return optimized RNG structures"""
	wordlen, exponent, start_id, max_id, seed = validate_parameters(**kwds)
	ptr, count, _ = create_mt_structs(wordlen, exponent, start_id, max_id, seed)

	# being extra-paranoidal
	try:
		mts_common = mt_common()
		mts_stripped = (mt_stripped * count)()
		fill_mt_structs_stripped(addressof(mts_common), addressof(mts_stripped), ptr, count)
	finally:
		free_mt_structs(ptr, count)

	return mts_common, mts_stripped
