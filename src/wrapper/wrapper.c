#include <Python.h>
#include <dc.h>

PyMODINIT_FUNC init_libdcmt(void);

static PyObject* class_mt_struct = NULL;
static PyObject* class_DcmtError = NULL;

static PyObject* dcmt_get_mt_parameter_st(PyObject *self, PyObject *args)
{
	int w, p;
	uint32_t seed;

	if(!PyArg_ParseTuple(args, "iiI:get_mt_parameter_st", &w, &p, &seed))
	{
			return NULL;
	}

	mt_struct *mts = get_mt_parameter_st(w, p, seed);
	if(NULL == mts)
	{
		PyErr_SetString(class_DcmtError, "Internal dcmt error occurred");
		return NULL;
	}

	// FIXME: original dcmt does not store the length of state vector anywhere,
	// so I have to use this formula taken from seive.c::init_mt_search()
	int state_len = p / w + 1;

	// build ctypes.Structure object
	//
	// FIXME: ideally, I would want get_mt_parameter_st() to write
	// directly into Structure object. It requires changing original dcmt.
	// Less ideally, I would want to copy from *mts to Structure directly.
	//
	// Note: discarding state vector part of mt_struct, because
	// it is not filled yet anyway
	PyObject *mt_struct_args = Py_BuildValue("s#i",
		(const char *)mts, sizeof(*mts), state_len);
	if(NULL == mt_struct_args)
	{
		return NULL;
	}

	PyObject *res = PyObject_CallObject(class_mt_struct, mt_struct_args);
	Py_DECREF(mt_struct_args);
	if(NULL == res)
	{
		return NULL;
	}

	return res;
}


static PyMethodDef dcmt_methods[] = {
	{"get_mt_parameter_st", dcmt_get_mt_parameter_st, METH_VARARGS,
		"Get structure with MT parameters"},
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

	class_mt_struct = PyObject_GetAttrString(dcmt_structures, "get_mt_struct");
	Py_DECREF(dcmt_structures);
	if(NULL == class_mt_struct)
		return;

	// import exception class

	PyObject *dcmt_exceptions = PyImport_ImportModule("dcmt.exceptions");
	if(NULL == dcmt_exceptions)
		return;

	class_DcmtError = PyObject_GetAttrString(dcmt_exceptions, "DcmtError");
	Py_DECREF(dcmt_exceptions);
	if(NULL == class_DcmtError)
		return;
}
