from libc.stdlib cimport malloc, free
from libc.limits cimport INT_MAX

cdef extern from "string.h":
	void *memcpy(void *s1, void *s2, int n)

cdef extern from "Python.h":
	void *PyMem_Malloc(size_t size)
	void PyMem_Free(void *buf)
	object _PyLong_FromByteArray(unsigned char* bytes, size_t n, int little_endian, int is_signed)
	unsigned long PyInt_AsUnsignedLongMask(object io)
	object PyInt_FromLong(long ival)
	long PyInt_AsLong(object io)
	object PyLong_FromUnsignedLong(unsigned long v)

cdef extern from "inttypes.h":
	ctypedef unsigned int uint32_t

cdef extern from "dc.h":

	ctypedef struct mt_struct:
		uint32_t aaa
		int mm,nn,rr,ww
		uint32_t wmask,umask,lmask
		int shift0, shift1, shiftB, shiftC
		uint32_t maskB, maskC
		int i
		uint32_t *state

	# new interface
	mt_struct *get_mt_parameter_id_st(int w, int p, int id, uint32_t seed)
	mt_struct **get_mt_parameters_st(int w, int p, int start_id, int max_id,
					 uint32_t seed, int *count)
	# common
	void free_mt_struct(mt_struct *mts)
	void free_mt_struct_array(mt_struct **mtss, int count)


cdef extern from "common.h":

	void sgenrand(uint32_t seed, mt_struct *mts)
	uint32_t random_uint32(mt_struct *mt)
	double random_float(mt_struct *mt)

from os import urandom as _urandom
from binascii import hexlify as _hexlify
import time


cdef uint32_t get_seed(object seed) except? -1:
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
			seed = long(time.time() * 256) # use fractional seconds
	elif not isinstance(seed, int) and not isinstance(seed, long):
		try:
			seed = hash(seed)
		except TypeError:
			raise DcmtParameterError("Seed must be an integer or hashable type")

	# not checking for overflow now, since we need 4 lower bytes anyway
	return PyInt_AsUnsignedLongMask(seed)

cdef void validate_parameters(wordlen, exponent, start_id, max_id,
		int *c_wordlen, int *c_exponent, int *c_start_id, int *c_max_id) except *:
	"""Return valid parameter or raise an exception"""

	cdef int w, p, sid, mid

	w = <int>PyInt_AsLong(wordlen)
	p = <int>PyInt_AsLong(exponent)
	sid = <int>PyInt_AsLong(start_id)
	mid = <int>PyInt_AsLong(max_id)

	if sid > mid:
		raise DcmtParameterError("Starting ID must be equal to or lower than maximum ID")

	if sid < 0 or mid > 65535:
		raise DcmtParameterError("All IDs must lie between 0 and 65535")

	if w == 31 and p == 521 and sid <= 9 and mid >= 9:
		raise DcmtParameterError("Known bug: cannot create RNG for wordlen=31, exponent=521, id=9")

	if w != 31 and w != 32:
		raise DcmtParameterError("Word length must be equal to 31 or 32")

	if p not in (521, 607, 1279, 2203, 2281, 3217, 4253, 4423,
			9689, 9941, 11213, 19937, 21701, 23209, 44497):
		raise DcmtParameterError("Mersenne exponent is incorrect or not from a supported set")

	c_wordlen[0] = w
	c_exponent[0] = p
	c_start_id[0] = sid
	c_max_id[0] = mid

cdef object create_mt_range(args, wordlen, exponent, seed, mt_struct ***mts, int *count):

	cdef int w, p, mid, sid

	if len(args) == 1:
		start_id = 0
		max_id, = args
	elif len(args) == 2:
		start_id, max_id = args
	else:
		raise TypeError("range expected 1 or 2 positional arguments")

	max_id -= 1

	if max_id < start_id:
		return []

	validate_parameters(wordlen, exponent, start_id, max_id, &w, &p, &sid, &mid)
	cdef uint32_t s = get_seed(seed)

	cdef mt_struct **m = get_mt_parameters_st(w, p, sid, mid, s, count)

	if count[0] < max_id - start_id + 1 or mts == NULL:
		if mts != NULL:
			free_mt_struct_array(m, count[0])
		raise DcmtError("dcmt internal error: could not create all requested RNGs")

	mts[0] = m

	return None
