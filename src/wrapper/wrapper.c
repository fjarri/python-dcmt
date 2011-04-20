#include <Python.h>
#include <dc.h>
#include <stdbool.h>
#include <stddef.h>
#include <numpy/arrayobject.h>

// matches structure definition in dcmt.functions
typedef struct {
	int mm, nn, rr, ww;
	uint32_t wmask, umask, lmask;
	int shift0, shift1, shiftB, shiftC;
	int state_len;
} mt_common;

// matches structure definition in dcmt.functions
typedef struct {
	uint32_t aaa, maskB, maskC;
} mt_stripped;

PyMODINIT_FUNC init_libdcmt(void);

static PyObject* class_DcmtError = NULL; // dcmt.exceptions.DcmtError

// Modified genmtrand.c::sgenrand_mt(), taken from nVidia Cuda SDK 4.0
//
// Victor Podlozhnyuk @ 05/13/2007:
// 1) Fixed sgenrand_mt():
//    - Fixed loop indexing: 'i' variable was off by one.
//    - apply wmask right on the state element initialization instead
//      of separate loop, which could produce machine-dependent results(wrong).
// 2) Slightly reformatted sources to be included into CUDA SDK.
static void sgenrand_mt_modified(uint32_t seed, mt_struct *mts){
    int i;

    mts->state[0] = seed & mts->wmask;

    for(i = 1; i < mts->nn; i++){
        mts->state[i] = (UINT32_C(1812433253) * (mts->state[i - 1] ^ (mts->state[i - 1] >> 30)) + i) & mts->wmask;
        /* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
        /* In the previous versions, MSBs of the seed affect   */
        /* only MSBs of the array mt[].                        */
    }
    mts->i = mts->nn;
}

// Extract pointer value from int or long
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

// Exported function: create MT generator structures
static PyObject* dcmt_create_mt_structs(PyObject *self, PyObject *args)
{
	int w, p, start_id, max_id;
	unsigned int int_seed;

	// parse arguments; overflow checking is the responsibility of caller

	if(!PyArg_ParseTuple(args, "iiiiI:create_mt_structs",
			&w, &p, &start_id, &max_id, &int_seed))
		return NULL;

	// just a precaution; seed is guaranteed to fit 32 bits
	uint32_t seed = (uint32_t)int_seed;

	int count = -1;
	mt_struct **mts = get_mt_parameters_st(w, p, start_id, max_id, seed, &count);
	if(NULL == mts || 0 == count)
	{
		PyErr_SetString(class_DcmtError, "Internal dcmt error occurred");
		return NULL;
	}

	PyObject *ptr = PyLong_FromVoidPtr((void *)mts);
	PyObject *res = Py_BuildValue("Oi", ptr, count);
	Py_DECREF(ptr);
	return res;
}

// Exported function: free original MT generator structures
static PyObject* dcmt_free_mt_structs(PyObject *self, PyObject *args)
{
	PyObject *ptr_obj = NULL;
	int count = 0;

	if(!PyArg_ParseTuple(args, "Oi:free_mt_structs", &ptr_obj, &count))
		return NULL;

	mt_struct **ptr = (mt_struct **)PyLong_AsVoidPtr(ptr_obj);
	if(NULL == ptr)
		return NULL;

	free_mt_struct_array(ptr, count);

	Py_RETURN_NONE;
}

// Exported function: fill Python MT generator structures
static PyObject* dcmt_fill_mt_structs(PyObject *self, PyObject *args)
{
	PyObject *mts_address_obj = NULL;
	PyObject *ptr_obj = NULL;
	int elem_size = 0, vec_offset = 0, count = 0;

	if(!PyArg_ParseTuple(args, "OiiOi:fill_mt_structs",
			&mts_address_obj, &elem_size, &vec_offset, &ptr_obj, &count))
		return NULL;

	mt_struct **ptr = (mt_struct **)PyLong_AsVoidPtr(ptr_obj);
	if(NULL == ptr)
		return NULL;

	char *mts_ptr = NULL;
	if(!parse_pointer(mts_address_obj, (void **)&mts_ptr))
		return NULL;

	// Fill structures
	for(int i = 0; i < count; i++)
	{
		mt_struct *elem = (mt_struct *)(mts_ptr + elem_size * i);

		// discarding state vector part of mt_struct, as well as
		// mt_struct.i field, since they are filled only on initialization
		memcpy(elem, ptr[i], offsetof(mt_struct, i));

		// Pointer to state vector will be substituted during calls to
		// sgenrand_mt()/genrand_mt()
		// In the meantime we are keeping offset to state vector in 'state' field
		// and not the actual pointer to allow user copy this struct in Python freely.
		elem->state = (uint32_t*)(size_t)vec_offset;
	}

	Py_RETURN_NONE;
}

// Exported function: fill stripped MT structures
// (one for common parameters, and small structures with unique parameters for each RNG)
static PyObject* dcmt_fill_mt_structs_stripped(PyObject *self, PyObject *args)
{
	PyObject *mts_common_address_obj = NULL;
	PyObject *mts_stripped_address_obj = NULL;
	PyObject *ptr_obj = NULL;
	int count = 0;

	if(!PyArg_ParseTuple(args, "OOOi:fill_mt_structs",
			&mts_common_address_obj, &mts_stripped_address_obj, &ptr_obj, &count))
		return NULL;

	mt_struct **ptr = (mt_struct **)PyLong_AsVoidPtr(ptr_obj);
	if(NULL == ptr)
		return NULL;

	mt_common *mts_common_ptr = NULL;
	if(!parse_pointer(mts_common_address_obj, (void **)&mts_common_ptr))
		return NULL;

	mt_stripped *mts_stripped_ptr = NULL;
	if(!parse_pointer(mts_stripped_address_obj, (void **)&mts_stripped_ptr))
		return NULL;

	// fill common parameters
	mts_common_ptr->mm = ptr[0]->mm;
	mts_common_ptr->nn = ptr[0]->nn;
	mts_common_ptr->rr = ptr[0]->rr;
	mts_common_ptr->ww = ptr[0]->ww;
	mts_common_ptr->umask = ptr[0]->umask;
	mts_common_ptr->wmask = ptr[0]->wmask;
	mts_common_ptr->lmask = ptr[0]->lmask;
	mts_common_ptr->shift0 = ptr[0]->shift0;
	mts_common_ptr->shift1 = ptr[0]->shift1;
	mts_common_ptr->shiftB = ptr[0]->shiftB;
	mts_common_ptr->shiftC = ptr[0]->shiftC;

	// fill unique parameters
	for(int i = 0; i < count; i++)
	{
		// discarding state vector part of mt_struct, as well as
		// mt_struct.i field, since they are filled only on initialization
		mts_stripped_ptr[i].aaa = ptr[i]->aaa;
		mts_stripped_ptr[i].maskB = ptr[i]->maskB;
		mts_stripped_ptr[i].maskC = ptr[i]->maskC;
	}

	Py_RETURN_NONE;
}

// Exported function: initialize MT generator with a seed
static PyObject* dcmt_init_mt_struct(PyObject *self, PyObject *args)
{
	PyObject *mt_address_obj = NULL;
	unsigned int int_seed;

	if(!PyArg_ParseTuple(args, "OI:init_mt_struct",
			&mt_address_obj, &int_seed))
		return NULL;

	// just a precaution; seed is guaranteed to fit 32 bits
	uint32_t seed = (uint32_t)int_seed;

	mt_struct *mt_ptr = NULL;
	if(!parse_pointer(mt_address_obj, (void **)&mt_ptr))
		return NULL;

	// temporarily substitute offset with real pointer which dcmt expects
	// FIXME: remove this trick by fixing dcmt or by using list instead of
	// ctypes array in Python part (so that I can set copy constructor etc)
	size_t offset = (size_t)mt_ptr->state;
	mt_ptr->state = (uint32_t*)((char*)mt_ptr + offset);
	sgenrand_mt_modified(seed, mt_ptr);
	mt_ptr->state = (uint32_t*)offset;

	Py_RETURN_NONE;
}

// Exported function: fill numpy array with random numbers
static PyObject* dcmt_fill_rand_int(PyObject *self, PyObject *args)
{
	PyObject *mt_address_obj = NULL;
	PyArrayObject *arr = NULL;

	if(!PyArg_UnpackTuple(args, "fill_rand_int", 2, 2, &mt_address_obj, &arr))
		return NULL;

	size_t size = 1;
	for(int i = 0; i < arr->nd; i++)
		size *= arr->dimensions[i];

	mt_struct *mt_ptr = NULL;
	if(!parse_pointer(mt_address_obj, (void **)&mt_ptr))
		return NULL;

	// temporarily substitute offset with real pointer which dcmt expects
	// FIXME: remove this trick by fixing dcmt or by using list instead of
	// ctypes array in Python part (so that I can set copy constructor etc)
	size_t offset = (size_t)mt_ptr->state;
	mt_ptr->state = (uint32_t*)((char*)mt_ptr + offset);
	for(int i = 0; i < size; i++)
		((uint32_t*)(arr->data))[i] = genrand_mt(mt_ptr);
	mt_ptr->state = (uint32_t*)offset;

	Py_RETURN_NONE;
}

static PyMethodDef dcmt_methods[] = {
	{"create_mt_structs", dcmt_create_mt_structs, METH_VARARGS,
		"Create C structures with MT RNGs"},
	{"fill_mt_structs", dcmt_fill_mt_structs, METH_VARARGS,
		"Fill Python structures with MT RNGs"},
	{"free_mt_structs", dcmt_free_mt_structs, METH_VARARGS,
		"Free C structures with MT RNGs"},
	{"fill_mt_structs_stripped", dcmt_fill_mt_structs_stripped, METH_VARARGS,
		"Fill stripped Python structures with MT RNGs"},
	{"init_mt_struct", dcmt_init_mt_struct, METH_VARARGS,
		"Initialize Python MT RNG structures with a seed"},
	{"fill_rand_int", dcmt_fill_rand_int, METH_VARARGS,
		"Fill numpy array with random numbers"},
	{NULL, NULL}
};

PyMODINIT_FUNC init_libdcmt(void)
{
	PyObject *self = Py_InitModule3("_libdcmt", dcmt_methods,
		"Dynamic creation of Mersenne twister RNGs.");
	if(NULL == self)
		return;

	// this is a macro, containing error checks inside
	import_array();

	PyObject *dcmt_exceptions = PyImport_ImportModule("dcmt.exceptions");
	if(NULL == dcmt_exceptions)
		return;

	class_DcmtError = PyObject_GetAttrString(dcmt_exceptions, "DcmtError");
	Py_DECREF(dcmt_exceptions);
	if(NULL == class_DcmtError)
		return;
}
