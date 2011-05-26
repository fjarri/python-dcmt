include "common.pxi"

import numpy

from dcmt.exceptions import DcmtError, DcmtParameterError

cdef extern from "numpy/arrayobject.h":

	ctypedef int npy_intp

	ctypedef extern class numpy.dtype [object PyArray_Descr]:
		cdef int type_num, elsize, alignment
		cdef char type, kind, byteorder, hasobject
		cdef object fields, typeobj

	ctypedef extern class numpy.ndarray [object PyArrayObject]:
		cdef char *data
		cdef int nd
		cdef npy_intp *dimensions
		cdef npy_intp *strides
		cdef object base
		cdef dtype descr
		cdef int flags

	cdef enum NPY_TYPES:
		NPY_BOOL
		NPY_BYTE
		NPY_UBYTE
		NPY_SHORT
		NPY_USHORT
		NPY_INT
		NPY_UINT
		NPY_LONG
		NPY_ULONG
		NPY_LONGLONG
		NPY_ULONGLONG
		NPY_FLOAT
		NPY_DOUBLE
		NPY_LONGDOUBLE
		NPY_CFLOAT
		NPY_CDOUBLE
		NPY_CLONGDOUBLE
		NPY_OBJECT
		NPY_STRING
		NPY_UNICODE
		NPY_VOID
		NPY_NTYPES
		NPY_NOTYPE

	npy_intp PyArray_SIZE(ndarray arr)
	object PyArray_ContiguousFromObject(object obj, NPY_TYPES type,
		int mindim, int maxdim)
	int PyArray_Check(object obj)

	void import_array()


# Initialize numpy
import_array()

cdef object uint32_to_python(uint32_t x):
	if x > 0x7FFFFFFF and INT_MAX < 0xFFFFFFFF:
		return PyLong_FromUnsignedLong(x)
	else:
		return PyInt_FromLong(x)

cdef uint32_t uint32_from_python(object x):
	return PyInt_AsUnsignedLongMask(x)

cdef get_mt_struct_fields(mt_struct *mt):
	common_fields = get_mt_struct_common_fields(mt)
	common_fields['aaa'] = uint32_to_python(mt.aaa)
	common_fields['maskB'] = uint32_to_python(mt.maskB)
	common_fields['maskC'] = uint32_to_python(mt.maskC)
	common_fields['i'] = int(mt.i)
	return common_fields

cdef get_mt_struct_common_fields(mt_struct *mt):
	return {
		'mm': int(mt.mm),
		'nn': int(mt.nn),
		'rr': int(mt.rr),
		'ww': int(mt.ww),
		'wmask': uint32_to_python(mt.wmask),
		'umask': uint32_to_python(mt.umask),
		'lmask': uint32_to_python(mt.lmask),
		'shift0': int(mt.shift0),
		'shift1': int(mt.shift1),
		'shiftB': int(mt.shiftB),
		'shiftC': int(mt.shiftC)
	}

cdef set_mt_struct_fields(mt_struct *mt, mt_dict):
	set_mt_struct_common_fields(mt, mt_dict)
	mt.aaa = uint32_from_python(mt_dict['aaa'])
	mt.maskB = uint32_from_python(mt_dict['maskB'])
	mt.maskC = uint32_from_python(mt_dict['maskC'])
	mt.i = uint32_from_python(mt_dict['i'])

cdef set_mt_struct_common_fields(mt_struct *mt, common_mt_dict):
	mt.mm = <int>common_mt_dict['mm']
	mt.nn = <int>common_mt_dict['nn']
	mt.rr = <int>common_mt_dict['rr']
	mt.ww = <int>common_mt_dict['ww']
	mt.wmask = uint32_from_python(common_mt_dict['wmask'])
	mt.umask = uint32_from_python(common_mt_dict['umask'])
	mt.lmask = uint32_from_python(common_mt_dict['lmask'])
	mt.shift0 = <int>common_mt_dict['shift0']
	mt.shift1 = <int>common_mt_dict['shift1']
	mt.shiftB = <int>common_mt_dict['shiftB']
	mt.shiftC = <int>common_mt_dict['shiftC']


cdef class DcmtRandomState:

	cdef mt_struct *mt

	def __init__(self, *args, wordlen=32, exponent=521, id=0, gen_seed=None):
		cdef int w, p, mid, sid
		validate_parameters(wordlen, exponent, id, id, &w, &p, &sid, &mid)
		cdef uint32_t s = get_seed(gen_seed)

		self.mt = get_mt_parameter_id_st(w, p, sid, s)
		self.seed(*args)

	def __dealloc__(self):
		if self.mt != NULL:
			free_mt_struct(self.mt)
			self.mt = NULL

	def __getstate__(self):
		return self.get_state()

	def __setstate__(self, state):
		self.set_state(state)

	def __reduce__(self):
		return self.__class__, (), self.get_state()

	def seed(self, seed=None):
		cdef uint32_t s = get_seed(seed)
		sgenrand(s, self.mt)

	def get_state(self):
		cdef ndarray state "arrayObject_state"
		cdef mt_struct *mt = <mt_struct *>self.mt
		state = <ndarray>numpy.empty(mt.nn, numpy.uint32)
		memcpy(<void*>(state.data), <void*>(mt.state), mt.nn * sizeof(uint32_t))
		return (1, get_mt_struct_fields(mt), state)

	def set_state(self, state):
		version, fields, state_vec = state
		assert version == 1, "State version " + str(version) + " is not supported"

		cdef mt_struct *mt = <mt_struct *>malloc(sizeof(mt_struct))
		cdef ndarray obj "arrayObject_obj"

		set_mt_struct_fields(mt, fields)

		mt.state = <uint32_t *>malloc(sizeof(uint32_t) * mt.nn)

		obj = <ndarray>PyArray_ContiguousFromObject(state_vec, NPY_UINT, 1, 1)
		if obj.dimensions[0] != mt.nn:
			raise ValueError("wrong state vector size")
		memcpy(<void*>(mt.state), <void*>(obj.data), mt.nn * sizeof(uint32_t))
		self.mt = mt

	def rand(self, *size):
		if len(size) == 0:
			return self.random_sample()
		else:
			return self.random_sample(size=size)

	def random_sample(self, size=None):
		if size is None:
			return PyFloat_FromDouble(random_float(self.mt))
		else:
			array = numpy.empty(size, numpy.float64)
			self.rand_fill(array)
			return array

	def rand_fill(self, arr):
		cdef double *array_data
		cdef ndarray array "arrayObject"
		cdef long length
		cdef long i

		if not PyArray_Check(arr):
			raise TypeError("function requires numpy array")

		array = <ndarray>arr
		if array.descr.type_num != NPY_DOUBLE:
			raise TypeError("function requires numpy array of type float64")

		length = PyArray_SIZE(array)
		array_data = <double *>array.data
		for i in range(length):
			array_data[i] = random_float(self.mt)

	@classmethod
	def range(cls, *args, wordlen=32, exponent=521, gen_seed=None):
		cdef int i, count
		cdef mt_struct **mts = NULL

		res = create_mt_range(args, wordlen, exponent, gen_seed, &mts, &count)
		if res != None:
			return res

		cdef DcmtRandomState rng
		rngs = []
		for i in range(count):
			rng = <DcmtRandomState>cls.__new__(cls)
			rng.mt = mts[i]
			rng.seed()

			rngs.append(rng)

		# We do not need array of pointers anymore, but we are keeping pointers to mt_struct's
		# So we are not using free_mt_struct_array()
		free(mts)

		return rngs

	@classmethod
	def from_mt_range(cls, mt_common, mt_unique):

		cdef mt_struct *mt
		cdef DcmtRandomState rng
		cdef ndarray unique_fields "arrayObject_unique"
		unique_fields = <ndarray>mt_unique
		cdef uint32_t *data = <uint32_t *>unique_fields.data

		rngs = []
		for i in xrange(mt_unique.shape[0]):
			rng = <DcmtRandomState>cls.__new__(cls)

			mt = <mt_struct *>malloc(sizeof(mt_struct))

			set_mt_struct_common_fields(mt, mt_common)

			mt.aaa = data[i * 4 + 0]
			mt.maskB = data[i * 4 + 1]
			mt.maskC = data[i * 4 + 2]
			mt.i = <int>data[i * 4 + 3]

			mt.state = <uint32_t *>malloc(sizeof(uint32_t) * mt.nn)

			rng.mt = mt
			rng.seed()

			rngs.append(rng)

		return rngs


def mt_range(*args, wordlen=32, exponent=521, gen_seed=None):

	cdef int i, count
	cdef mt_struct **mts = NULL

	res = create_mt_range(args, wordlen, exponent, gen_seed, &mts, &count)
	if res != None:
		return res

	common_fields = get_mt_struct_common_fields(mts[0])

	cdef ndarray unique_fields "arrayObject_unique"
	unique_fields = <ndarray>numpy.empty((count, 4), numpy.uint32)

	cdef uint32_t *data = <uint32_t *>unique_fields.data
	for i in range(count):
		data[i * 4 + 0] = mts[i].aaa
		data[i * 4 + 1] = mts[i].maskB
		data[i * 4 + 2] = mts[i].maskC
		data[i * 4 + 3] = <uint32_t>mts[i].i

	free_mt_struct_array(mts, count)

	return common_fields, unique_fields
