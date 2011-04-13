# module functional interface:
#
# mt_struct *get_mt_parameter_st(int w, int p, uint32_t seed)
# returns ctypes.Structure with mt_struct fields
# checks that w and p are good and throws exception if not
#
# mt_struct *get_mt_parameter_id_st(int w, int p, int id, uint32_t seed)
# returns ctypes.Structure with mt_struct fields
# same checks as for previous one + checks that id < 65536
#
# mt_struct **get_mt_parameters_st(int w, int p, int start_id,
#	int max_id, uint32_t seed)
# returns ctypes.Array of ctypes.Structure with mt_struct fields
# same checks as for previous one + checks that start_id and max_id are valid +
# checks that there is no id=9 with w=31 and p=521 (known bug)
# Note: does not return count, since user can just take len(return value)
#
# void sgenrand_mt(uint32_t seed, mt_struct *mts)
# takes ctypes.Structure (from get_mt_parameter*) as a second parameter
#
# uint32_t genrand_mt(mt_struct *mts)
# takes ctypes.Structure (from get_mt_parameter*) as a second parameter
#
#
# additional functions:
#
# def get_mt_parameters_st_stripped(int w, int p, int start_id,
#	int max_id, uint32_t seed)
# return tuple of two values:
# - ctypes.Structure with common parameters
# - ctypes.Array of ctypes.Structure with unique parameters for each generator
# same checks as for previous one

from ctypes import *


class mt_struct(Structure):

	_fields_ = [
		("aaa", c_uint),
		("mm", c_int),
		("nn", c_int),
		("rr", c_int),
		("ww", c_int),
		("wmask", c_uint),
		("umask", c_uint),
		("lmask", c_uint),
		("shift0", c_int),
		("shift1", c_int),
		("shiftB", c_int),
		("shiftC", c_int),
		("maskB", c_uint),
		("maskC", c_uint),
		("i", c_int),
		("state", POINTER(c_uint))
	]

	def __init__(self, raw_data, state_data):
		Structure.__init__(self)
		memmove(addressof(self), raw_data, sizeof(self))

		state_class = c_uint * (len(state_data) / sizeof(c_uint))
		state = state_class()
		memmove(addressof(state), state_data, sizeof(state))
		self.state = state
