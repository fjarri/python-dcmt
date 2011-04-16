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
		PyErr_SetString(class_DcmtError, "Seed should be a subtype of int or long");
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
		PyErr_SetString(class_DcmtError, "Pointer should be a subtype of int or long");
		return false;
	}

	*ptr = (void *)long_ptr;
	return true;
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

static PyObject* dcmt_create_generators(PyObject *self, PyObject *args, PyObject *kwds)
{
	char* keywords[] = {"bits", "power", "start_id", "max_id", "seed", NULL};
	int w = 32, p = 521, start_id = 0, max_id = 0;
	PyObject *seed_obj = NULL;

	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|iiiiO:create_generators", keywords,
			&w, &p, &start_id, &max_id, &seed_obj))
		return NULL;

	uint32_t seed;
	if(!parse_seed(seed_obj, &seed))
		return NULL;

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

	PyObject *func_args = Py_BuildValue("(O)", mt_obj);
	if(NULL == func_args)
		return NULL;

	PyObject *res = PyObject_CallObject(func_addressof, func_args);
	Py_DECREF(func_args);
	if(NULL == res)
		return NULL;

	mt_struct *mt_ptr = NULL;
	if(!parse_pointer(res, (void **)&mt_ptr))
	{
		Py_DECREF(res);
		return NULL;
	}
	Py_DECREF(res);

	size_t offset = (size_t)mt_ptr->state;
	mt_ptr->state = (uint32_t*)((char*)mt_ptr + offset);
	sgenrand_mt(seed, mt_ptr);
	mt_ptr->state = (uint32_t*)offset;

	Py_RETURN_NONE;
}

static PyMethodDef dcmt_methods[] = {
	{"create_generators", dcmt_create_generators, METH_VARARGS | METH_KEYWORDS,
		"Get structure(s) with MT parameters"},
	{"init_generator", dcmt_init_generator, METH_VARARGS | METH_KEYWORDS,
		"Initialize MT generator with seed"},
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
