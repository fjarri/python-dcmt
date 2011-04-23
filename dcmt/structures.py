from ctypes import Structure, POINTER, c_int, c_uint32

# matches structure definitions in C extension
class mt_common(Structure):
	_fields_ = [
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
		("shiftC", c_int)
	]

# matches structure definitions in C extension
class mt_stripped(Structure):
	_fields_ = [
		("aaa", c_uint32),
		("maskB", c_uint32),
		("maskC", c_uint32),
		("i", c_int)
	]

# Original library allocates two separate pieces of memory on a heap,
# because the size of mt_struct depends on state vector size.
# Since I will not use dcmt functions to delete mt_struct instances,
# it is more convenient to keep everything in one struct,
# using dynamic features of Python to build necessary class.
#
# This class will serve as a base to determine alignment;
# state_vec will be extended according to given RNG parameters
class mt_struct_base(Structure):
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
		("state_vec", c_uint32)
	]

def generate_declaration(struct_cls):
	type_mapping = {
		c_uint32: 'uint32_t',
		c_int: 'int',
		POINTER(c_uint32): 'uint32_t*',
	}

	struct_name = struct_cls.__name__
	res = "typedef struct _" + struct_name + " {\n"

	for name, tp in struct_cls._fields_:
		if tp in type_mapping:
			res += "\t" + type_mapping[tp] + " " + name + ";\n"
		else:
			raise Exception("Unknown type: " + str(tp))

	res += "} " + struct_name + ";\n"

	return res

def generate_header():
	declarations = [generate_declaration(x) for x in
		(mt_common, mt_stripped, mt_struct_base)]
	return "\n".join(declarations)
