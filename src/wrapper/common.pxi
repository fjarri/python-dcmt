from libc.stdlib cimport malloc, free

cdef extern from "string.h":
	void *memcpy(void *s1, void *s2, int n)

cdef extern from "Python.h":
	void *PyMem_Malloc(size_t size)
	void PyMem_Free(void *buf)
	object _PyLong_FromByteArray(unsigned char* bytes, size_t n, int little_endian, int is_signed)

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

# FIXME: on 32-bit systems taking int(uint32_t) is incorrect
cdef get_mt_struct_fields(mt_struct *mt):
	return {
		'aaa': int(mt.aaa),
		'mm': int(mt.mm),
		'nn': int(mt.nn),
		'rr': int(mt.rr),
		'ww': int(mt.ww),
		'wmask': int(mt.wmask),
		'umask': int(mt.umask),
		'lmask': int(mt.lmask),
		'shift0': int(mt.shift0),
		'shift1': int(mt.shift1),
		'shiftB': int(mt.shiftB),
		'shiftC': int(mt.shiftC),
		'maskB': int(mt.maskB),
		'maskC': int(mt.maskC),
		'i': int(mt.i)
	}

cdef set_mt_struct_fields(mt_struct *mt, mt_dict):
	mt.aaa = <uint32_t>mt_dict['aaa']
	mt.mm = <int>mt_dict['mm']
	mt.nn = <int>mt_dict['nn']
	mt.rr = <int>mt_dict['rr']
	mt.ww = <int>mt_dict['ww']
	mt.wmask = <uint32_t>mt_dict['wmask']
	mt.umask = <uint32_t>mt_dict['umask']
	mt.lmask = <uint32_t>mt_dict['lmask']
	mt.shift0 = <int>mt_dict['shift0']
	mt.shift1 = <int>mt_dict['shift1']
	mt.shiftB = <int>mt_dict['shiftB']
	mt.shiftC = <int>mt_dict['shiftC']
	mt.maskB = <uint32_t>mt_dict['maskB']
	mt.maskC = <uint32_t>mt_dict['maskC']
	mt.i = <int>mt_dict['i']

cdef set_mt_struct_common_fields(mt_struct *mt, common_mt_dict):
	mt.mm = <int>common_mt_dict['mm']
	mt.nn = <int>common_mt_dict['nn']
	mt.rr = <int>common_mt_dict['rr']
	mt.ww = <int>common_mt_dict['ww']
	mt.wmask = <uint32_t>common_mt_dict['wmask']
	mt.umask = <uint32_t>common_mt_dict['umask']
	mt.lmask = <uint32_t>common_mt_dict['lmask']
	mt.shift0 = <int>common_mt_dict['shift0']
	mt.shift1 = <int>common_mt_dict['shift1']
	mt.shiftB = <int>common_mt_dict['shiftB']
	mt.shiftC = <int>common_mt_dict['shiftC']


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

	if type(seed) not in (int, long):
		try:
			seed = hash(seed)
		except TypeError:
			raise DcmtParameterError("Seed must be an integer or hashable type")

	return seed & 0xFFFFFFFF

	return seed

def validate_parameters(wordlen, exponent, start_id, max_id):
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
