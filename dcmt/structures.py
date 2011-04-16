from ctypes import *

# Original library allocates two separate pieces of memory on a heap,
# because the size of mt_struct depends on state vector size.
# Since I will not use dcmt functions to delete mt_struct instances,
# it is more convenient to keep everything in one struct,
# using dynamic features of Python to build necessary class.
def get_mt_structs(state_len, num):

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

	mts = (mt_struct * num)()

	# last two numbers are required to account for alignment in extension
	return mts, sizeof(mt_struct), mt_struct.state_vec.offset
