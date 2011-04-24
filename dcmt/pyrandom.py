from random import Random
from ctypes import addressof

from .functions import create_mts, init_mt
from ._libdcmt import random, jumpahead, getrandbits


class _DcmtRandom(Random):

	def __init__(self, mt):
		self._mt_struct = mt
		self._mt_ptr = addressof(self._mt_struct)
		super(_DcmtRandom, self).__init__()

	def random(self):
		"""Get the next random number in the range [0.0, 1.0)."""
		return random(self._mt_ptr)

	def seed(self, a=None):
		init_mt(self._mt_struct, seed=a)

	def getstate(self):
		"""Return internal state; can be passed to setstate() later."""
		fields = []
		for name, _ in self._mt_struct._fields_:
			if name == 'state':
				fields.append(0)
			elif name == 'state_vec':
				fields.append(tuple(i for i in self._mt_struct.state_vec))
			else:
				fields.append(getattr(self._mt_struct, name))

	 	return (1, tuple(fields))

	def setstate(self, state):
		"""Restore internal state from object returned by getstate()."""
		version, fields = state
		assert version == 1, "State version " + str(version) + " is not supported"

		for field, field_pair in zip(fields, self._mt_struct._fields_):
			name, tp = field_pair
			if name == 'state':
				pass
			elif name == 'state_vec':
				for i, e in enumerate(field):
					self._mt_struct.state_vec[i] = e
			else:
				setattr(self._mt_struct, name, tp(field))

		self._mt_ptr = addressof(self._mt_struct)

	def jumpahead(self, n):
		"""Jump to a distant state"""
		jumpahead(self._mt_ptr, n)

	def getrandbits(self, k):
		"""getrandbits(k) -> x. Generates a long int with k random bits."""
		return getrandbits(self._mt_ptr)


class DcmtRandom(_DcmtRandom):

	def __init__(self, id=0, **kwds):
		mt_params = dict(start_id=id, max_id=id)
		for key in ('wordlen', 'exponent', 'seed'):
			if key in kwds:
				mt_params[key] = kwds[key]

		mt = create_mts(**mt_params)
		super(DcmtRandom, self).__init__(mt[0])
