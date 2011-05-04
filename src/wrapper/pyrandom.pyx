include "common.pxi"

from random import Random

from dcmt.exceptions import DcmtError, DcmtParameterError


cdef class RandomContainer:

	cdef mt_struct *mt

	def __init__(self):
		self.mt = NULL

	cdef initWithStruct(self, mt_struct *mt):
		self.mt = mt

	cdef initWithParams(self, int wordlen, int exponent, int id, int seed):
		self.mt = get_mt_parameter_id_st(wordlen, exponent, id, seed)

	def __dealloc__(self):
		if self.mt != NULL:
			free_mt_struct(self.mt)
			self.mt = NULL

	cdef seed(self, uint32_t seed):
		sgenrand(seed, self.mt)

	cdef random(self):
		return random_float(self.mt)

	cdef random_raw(self):
		return random_uint32(self.mt)

	def jumpahead(self, n):

		if type(n) not in (int, long):
			raise TypeError("jumpahead requires an integer, not " + str(type(n)))

		cdef uint32_t *state = self.mt.state
		cdef uint32_t i, j
		cdef uint32_t tmp
		cdef int nn = self.mt.nn

		for i in range(nn - 1, 1, -1):
			j = n % int(i)
			tmp = state[i]
			state[i] = state[j]
			state[j] = tmp

		for i in range(nn):
			state[i] += i + 1

		self.mt.i = nn

	def getrandbits(self, kk):
		# WARNING: currently works only for 31 and 32 bit randoms

		cdef int k = <int>kk, i, j, bytes
		cdef uint32_t r
		cdef unsigned char *bytearray

		if k <= 0:
			raise ValueError("number of bits must be greater than zero")

		bytes = ((k - 1) / 32 + 1) * 4
		bytearray = <unsigned char *>PyMem_Malloc(bytes);
		if bytearray == NULL:
			raise MemoryError()

	    # Fill-out whole words, byte-by-byte to avoid endianness issues
		cdef int bytelen = 4 if self.mt.ww == 32 else 3
		cdef int bitlen = bytelen * 8

		for i in range(0, bytes, bytelen):
			r = random_uint32(self.mt)
			if k < bitlen:
				r >>= (bitlen - k)

			for j in range(bytelen):
				bytearray[i+j] = <unsigned char>(r >> (8 * j))

			k -= bitlen

	    # little endian order to match bytearray assignment order
		result = _PyLong_FromByteArray(bytearray, bytes, 1, 0)
		PyMem_Free(bytearray)
		return result

	def getstate(self):
		"""Return internal state; can be passed to setstate() later."""
		cdef mt_struct *mt = self.mt

		state = []
		for i in range(self.mt.nn):
			state.append(mt.state[i])

		return (1, (mt.aaa, mt.mm, mt.nn, mt.rr, mt.ww,
			mt.wmask, mt.umask, mt.lmask,
			mt.shift0, mt.shift1, mt.shiftB, mt.shiftC,
			mt.maskB, mt.maskC, mt.i, tuple(state)))

	def setstate(self, state):
		"""Restore internal state from object returned by getstate()."""
		version, fields = state
		if version != 1:
			raise DcmtError("State version " + str(version) + " is not supported")

		cdef mt_struct *mt = <mt_struct *>malloc(sizeof(mt_struct))

		mt.aaa, mt.mm, mt.nn, mt.rr, mt.ww, \
			mt.wmask, mt.umask, mt.lmask, \
			mt.shift0, mt.shift1, mt.shiftB, mt.shiftC, \
			mt.maskB, mt.maskC, mt.i, state_vec = fields

		mt.state = <uint32_t *>malloc(sizeof(uint32_t) * mt.nn)
		for i in range(mt.nn):
			mt.state[i] = state_vec[i]

		self.mt = mt


class DcmtRandom(Random):

	def __init__(self, wordlen=32, exponent=521, id=0, seed=None):
		cdef int w, p, mid, sid
		cdef uint32_t s = get_seed(seed)

		validate_parameters(wordlen, exponent, id, id, &w, &p, &sid, &mid)

		self.mt = <RandomContainer>RandomContainer()
		self.mt.initWithParams(w, p, sid, s)

	def seed(self, a=None):
		cdef uint32_t seed = get_seed(a)
		cdef RandomContainer mt = <RandomContainer>self.mt
		mt.seed(seed)

	def random(self):
		cdef RandomContainer mt = <RandomContainer>self.mt
		return mt.random()

	def random_raw(self):
		cdef RandomContainer mt = <RandomContainer>self.mt
		return mt.random_raw()

	def getstate(self):
		cdef RandomContainer mt = <RandomContainer>self.mt
		return mt.getstate()

	def setstate(self, state):
		cdef RandomContainer mt = <RandomContainer>self.mt
		mt.setstate(state)

	def getrandbits(self, kk):
		cdef RandomContainer mt = <RandomContainer>self.mt
		return mt.getrandbits(kk)

	def jumpahead(self, n):
		cdef RandomContainer mt = <RandomContainer>self.mt
		mt.jumpahead(n)

	@classmethod
	def range(cls, *args, wordlen=32, exponent=521, seed=None):

		cdef int w, p, mid, sid

		if len(args) == 1:
			start_id = 0
			max_id = args[0] - 1
		elif len(args) == 2:
			start_id = args[0]
			max_id = args[1] - 1
		else:
			raise TypeError("range expected 1 or 2 positional arguments")

		if max_id < start_id:
			return []

		validate_parameters(wordlen, exponent, start_id, max_id, &w, &p, &sid, &mid)
		cdef uint32_t s = get_seed(seed)

		cdef int count, i
		cdef mt_struct **mts = get_mt_parameters_st(w, p, sid, mid, s, &count)

		if count < max_id - start_id + 1 or mts == NULL:
			raise DcmtError("dcmt internal error: could not create all requested RNGs")

		cdef RandomContainer rc
		rngs = []
		for i in range(count):
			rc = <RandomContainer>RandomContainer()
			rc.initWithStruct(mts[i])

			rng = cls.__new__(cls)
			cls.mt = rc

			rngs.append(rng)

		# We do not need array of pointers anymore, but we are keeping pointers to mt_struct's
		# So we are not using free_mt_struct_array()
		free(mts)

		return rngs
