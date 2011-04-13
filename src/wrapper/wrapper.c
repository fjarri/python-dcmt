#include <Python.h>
#include <dc.h>

PyMODINIT_FUNC init_libdcmt(void);

static PyObject* dcmt_structures = NULL;

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
		// raise Exception
		return NULL;
	}

	// build ctypes.Structure object

	return args;
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

	//dcmt_structures = PyImport_ImportModule("dcmt.structures");
}
