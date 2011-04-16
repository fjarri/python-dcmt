#include <Python.h>
#include <dc.h>
#include <stdbool.h>
#include <stddef.h>

PyMODINIT_FUNC init_libdcmt(void);

static PyObject* func_get_mt_structs = NULL;
static PyObject* func_addressof = NULL;
static PyObject* class_DcmtError = NULL;

static int parse_seed(PyObject *obj, uint32_t *num)
{
	if(NULL == obj || Py_None == obj)
	{
		*num = (uint32_t)time(NULL);
		return true;
	}

	if(PyInt_Check(obj))
	{
		long num_long = PyInt_AsLong(obj);
		if(-1 == num_long && PyErr_Occurred())
			return false;

		*num = (uint32_t)num_long;
		return true;
	}
	else if(PyLong_Check(obj))
	{
		long num_long = PyLong_AsLong(obj);
		if(-1 == num_long && PyErr_Occurred())
			return false;

		*num = (uint32_t)num_long;
		return true;
	}
	else
	{
		PyErr_SetString(class_DcmtError, "Seed must be a subtype of int or long");
		return false;
	}
}

static int parse_pointer(PyObject *obj, void **ptr)
{
	long long_ptr;
	if(PyInt_Check(obj))
	{
		long_ptr = PyInt_AsLong(obj);
		if(-1 == long_ptr && PyErr_Occurred())
			return false;
	}
	else if(PyLong_Check(obj))
	{
		long_ptr = PyLong_AsLong(obj);
		if(-1 == long_ptr && PyErr_Occurred())
			return false;
	}
	else
	{
		PyErr_SetString(class_DcmtError, "Pointer must be a subtype of int or long");
		return false;
	}

	*ptr = (void *)long_ptr;
	return true;
}

static int parse_addressof(PyObject *obj, void **ptr)
{
	PyObject *func_args = Py_BuildValue("(O)", obj);
	if(NULL == func_args)
		return false;

	PyObject *res = PyObject_CallObject(func_addressof, func_args);
	Py_DECREF(func_args);
	if(NULL == res)
		return false;

	int result = true;
	if(!parse_pointer(res, ptr))
		result = false;

	Py_DECREF(res);
	return result;
}

static int get_state_len(int w, int p)
{
	// FIXME: original dcmt does not store the length of state vector anywhere,
	// so I have to use this formula taken from seive.c::init_mt_search()
	return p / w + 1;
}

static PyObject *create_mt_array(int state_len, int count, void **array_ptr,
	long *elem_size, long *state_vec_offset)
{
	PyObject *func_args = Py_BuildValue("ii", state_len, count);
	if(NULL == func_args)
		return NULL;

	PyObject *res = PyObject_CallObject(func_get_mt_structs, func_args);
	Py_DECREF(func_args);
	if(NULL == res)
		return NULL;

	// Parse get_mt_structs() return value: (mts object,
	// address of mts object buffer, size of mts object buffer, offset of state vector)
	PyObject *array_obj = NULL;
	PyObject *array_ptr_obj = NULL;
	PyObject *elem_size_obj = NULL;
	PyObject *state_vec_offset_obj = NULL;
	if(!PyArg_UnpackTuple(res, "create_generators", 4, 4,
			&array_obj, &array_ptr_obj, &elem_size_obj, &state_vec_offset_obj))
	{
		Py_DECREF(res);
		return NULL;
	}

	if(!parse_pointer(array_ptr_obj, array_ptr))
	{
		Py_DECREF(res);
		return NULL;
	}

	// get element size
	*elem_size = PyInt_AsLong(elem_size_obj);
	if(-1 == *elem_size && PyErr_Occurred())
	{
		Py_DECREF(res);
		return NULL;
	}

	// get state vector offset
	*state_vec_offset = PyInt_AsLong(state_vec_offset_obj);
	if(-1 == *state_vec_offset && PyErr_Occurred())
	{
		Py_DECREF(res);
		return NULL;
	}

	Py_DECREF(array_ptr_obj);
	Py_DECREF(elem_size_obj);
	Py_DECREF(state_vec_offset_obj);

	return array_obj;
}


static int test_wordlen_validity(int w)
{
	int valid = (w == 31 || w == 32);
	if(!valid)
		PyErr_SetString(class_DcmtError, "Word length must be equal to 31 or 32");

	return valid;
}

static int test_exponent_validity(int p)
{
	int valid;

	// taken from seive.c::proper_mersenne_exponent()
    switch(p)
	{
	case 521:
	case 607:
	case 1279:
	case 2203:
	case 2281:
	case 3217:
	case 4253:
	case 4423:
	case 9689:
	case 9941:
	case 11213:
	case 19937:
	case 21701:
	case 23209:
	case 44497:
		valid = true;
		break;
    default:
		valid = false;
    }

	if(!valid)
		PyErr_SetString(class_DcmtError,
			"Mersenne exponent is incorrect or not from a supported set");

	return valid;
}

static int test_id_validity(int w, int p, int start_id, int max_id)
{
	if(start_id > max_id)
	{
		PyErr_SetString(class_DcmtError, "Starting ID must be equal to or lower than maximum ID");
		return false;
	}

	if(start_id < 0 || max_id > 65535)
	{
		PyErr_SetString(class_DcmtError, "All IDs must lie between 0 and 65535");
		return false;
	}

	// known bug - algorithm can't create generator for w=31, p=521, id=9
	if(w == 31 && p == 521 && start_id <= 9 && max_id >= 9)
	{
		PyErr_SetString(class_DcmtError,
			"Known bug: cannot create generator for wordlen=31, exponent=521, id=9");
		return false;
	}

	return true;
}

static PyObject* dcmt_create_generators(PyObject *self, PyObject *args, PyObject *kwds)
{
	char* keywords[] = {"wordlen", "exponent", "start_id", "max_id", "seed", NULL};
	int w = 32, p = 521, start_id = 0, max_id = 0;
	PyObject *seed_obj = NULL;

	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|iiiiO:create_generators", keywords,
			&w, &p, &start_id, &max_id, &seed_obj))
		return NULL;

	// test parameter validity

	if(!test_wordlen_validity(w)) return NULL;
	if(!test_exponent_validity(p)) return NULL;
	if(!test_id_validity(w, p, start_id, max_id)) return NULL;

	uint32_t seed;
	if(!parse_seed(seed_obj, &seed))
		return NULL;

	// get MT structures

	int count = -1;
	mt_struct **mts = get_mt_parameters_st(w, p, start_id, max_id, seed, &count);
	if(NULL == mts)
	{
		PyErr_SetString(class_DcmtError, "Internal dcmt error occurred");
		return NULL;
	}

	// FIXME: original dcmt does not store the length of state vector anywhere,
	// so I have to use this formula taken from seive.c::init_mt_search()
	int state_len = get_state_len(w, p);

	// build ctypes.Structure array
	//
	// FIXME: ideally, I would want get_mt_parameter_st() to write
	// directly into Structure object. It requires changing original dcmt.
	// Less ideally, I would want to copy from *mts to Structure directly.
	char *array_ptr = NULL;
	long elem_size = 0, state_vec_offset = 0;
	PyObject *array_obj = create_mt_array(state_len, count,
		(void **)&array_ptr, &elem_size, &state_vec_offset);
	if(NULL == array_obj)
	{
		free_mt_struct_array(mts, count);
		return NULL;
	}

	// Fill structures
	for(int i = 0; i < count; i++)
	{
		// discarding state vector part of mt_struct, as well as
		// mt_struct.i field, since they are filled only on initialization
		memcpy(array_ptr + elem_size * i, mts[i], offsetof(mt_struct, i));

		// This will be substituted before usage.
		// Keeping offset and not the actual pointer to allow
		// user copy this struct in Python freely.
		((mt_struct*)(array_ptr + elem_size * i))->state = (uint32_t*)state_vec_offset;
	}

	free_mt_struct_array(mts, count);

	return array_obj;
}

static PyObject* dcmt_init_generator(PyObject *self, PyObject *args, PyObject *kwds)
{
	char* keywords[] = {"mts", "seed", NULL};
	PyObject *mt_obj = NULL;
	PyObject *seed_obj = NULL;

	if(!PyArg_ParseTupleAndKeywords(args, kwds, "O|O:init_generator", keywords,
			&mt_obj, &seed_obj))
		return NULL;

	uint32_t seed;
	if(!parse_seed(seed_obj, &seed))
		return NULL;

	mt_struct *mt_ptr = NULL;
	if(!parse_addressof(mt_obj, (void **)&mt_ptr))
		return NULL;

	size_t offset = (size_t)mt_ptr->state;
	mt_ptr->state = (uint32_t*)((char*)mt_ptr + offset);
	sgenrand_mt(seed, mt_ptr);
	mt_ptr->state = (uint32_t*)offset;

	Py_RETURN_NONE;
}

static PyObject* dcmt_get_random(PyObject *self, PyObject *args)
{
	PyObject *mt_obj = NULL;

	if(!PyArg_ParseTuple(args, "O:get_random", &mt_obj))
		return NULL;

	mt_struct *mt_ptr = NULL;
	if(!parse_addressof(mt_obj, (void **)&mt_ptr))
		return NULL;

	size_t offset = (size_t)mt_ptr->state;
	mt_ptr->state = (uint32_t*)((char*)mt_ptr + offset);
	uint32_t num = genrand_mt(mt_ptr);
	mt_ptr->state = (uint32_t*)offset;

	return Py_BuildValue("I", num);
}

static PyMethodDef dcmt_methods[] = {
	{"create_generators", dcmt_create_generators, METH_VARARGS | METH_KEYWORDS,
		"Get structure(s) with MT parameters"},
	{"init_generator", dcmt_init_generator, METH_VARARGS | METH_KEYWORDS,
		"Initialize MT generator with seed"},
	{"get_random", dcmt_get_random, METH_VARARGS,
		"Get random value from a generator"},
	{NULL, NULL}
};

PyMODINIT_FUNC init_libdcmt(void)
{
	PyObject *self = Py_InitModule3("_libdcmt", dcmt_methods,
		"Dynamic creation of Mersenne twister RNGs.");
	if(NULL == self)
		return;

	// import structure classes from .structures module

	PyObject *dcmt_structures = PyImport_ImportModule("dcmt.structures");
	if(NULL == dcmt_structures)
		return;

	func_get_mt_structs = PyObject_GetAttrString(dcmt_structures, "get_mt_structs");
	Py_DECREF(dcmt_structures);
	if(NULL == func_get_mt_structs)
		return;

	// import exception class

	PyObject *dcmt_exceptions = PyImport_ImportModule("dcmt.exceptions");
	if(NULL == dcmt_exceptions)
		return;

	class_DcmtError = PyObject_GetAttrString(dcmt_exceptions, "DcmtError");
	Py_DECREF(dcmt_exceptions);
	if(NULL == class_DcmtError)
		return;

	// import ctypes.addressof

	PyObject *ctypes = PyImport_ImportModule("ctypes");
	if(NULL == ctypes)
		return;

	func_addressof = PyObject_GetAttrString(ctypes, "addressof");
	Py_DECREF(ctypes);
	if(NULL == func_addressof)
		return;
}
