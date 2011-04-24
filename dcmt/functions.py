import numpy
from time import time
from ctypes import Structure, addressof, sizeof
from os import urandom as _urandom
from binascii import hexlify as _hexlify

from _libdcmt import create_mt_structs, fill_mt_structs, \
	fill_mt_structs_stripped, init_mt_struct, fill_numpy_rand, \
	fill_numpy_randraw, free_mt_structs
from .exceptions import DcmtParameterError
from .structures import mt_common, mt_stripped, mt_struct_base


def get_seed(seed):
	"""
	Return valid seed or raise an exception

	Partially nicked from Python library.
	Don't want to move this to extension like they did though,
	because it is hardly a bottleneck.
	"""

	if seed is None:
		try:
			seed = long(_hexlify(_urandom(16)), 16)
		except NotImplementedError:
			import time
			seed = long(time.time() * 256) # use fractional seconds

	if type(seed) not in (int, long):
		try:
			seed = hash(seed)
		except TypeError:
			raise DcmtParameterError("Seed must be an integer or hashable type")

	return seed & (2 ** 32 - 1)

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

	return wordlen, exponent, start_id, max_id, get_seed(seed)

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
		fill_mt_structs(addressof(mts), sizeof(mt_struct), ptr, count)
	finally:
		free_mt_structs(ptr, count)

	return mts

def init_mt(mt, seed=None):
	"""Initialize RNG structure with given seed"""
	seed = get_seed(seed)
	init_mt_struct(addressof(mt), seed)

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

# numpy mimicking methods

def randraw(mt, *shape):
	"""Return numpy array with random integer numbers"""
	randoms = numpy.empty(*shape, dtype=numpy.uint32)
	fill_numpy_randraw(addressof(mt), randoms)
	return randoms

def rand(mt, *shape):
	"""Return numpy array with random floating-point numbers"""
	randoms = numpy.empty(*shape, dtype=numpy.float64)
	fill_numpy_rand(addressof(mt), randoms)
	return randoms

