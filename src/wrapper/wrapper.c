#include <Python.h>
#include <dc.h>
#include <stdbool.h>
#include <stddef.h>
#include <numpy/arrayobject.h>
#include "structures.h" // automatically generated in setup script

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

// Slightly optimised reference implementation of the Mersenne Twister,
// taken from numpy 1.5.1
// (replaced "x&1U ? aa : 0U" with "-(x & 1) & aa")
static uint32_t genrand_mt_modified(mt_struct *mts)
{
	uint32_t *st, uuu, lll, aa, x;
	int k,n,m,lim;

	if(mts->i == mts->nn)
	{
		n = mts->nn; m = mts->mm;
		aa = mts->aaa;
		st = mts->state;
		uuu = mts->umask; lll = mts->lmask;

		lim = n - m;
		for(k = 0; k < lim; k++)
		{
			x = (st[k] & uuu) | (st[k+1] & lll);
			st[k] = st[k+m] ^ (x>>1) ^ (-(x & 1) & aa);
		}

		lim = n - 1;
		for (; k < lim; k++)
		{
			x = (st[k] & uuu) | (st[k+1] & lll);
			st[k] = st[k+m-n] ^ (x>>1) ^ (-(x & 1) & aa);
		}

		x = (st[n-1] & uuu) | (st[0] & lll);
		st[n-1] = st[m-1] ^ (x >> 1) ^ (-(x & 1) & aa);

		mts->i = 0;
	}

	x = mts->state[mts->i];
	mts->i += 1;

	/* Tempering */
	x ^= (x >> mts->shift0);
	x ^= (x << mts->shiftB) & mts->maskB;
	x ^= (x << mts->shiftC) & mts->maskC;
	x ^= (x >> mts->shift1);

	return x;
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

// Convenience function to get mt_struct pointer from PyObject
// with refreshed state vector pointer
static mt_struct *parse_mt_struct_ptr(PyObject *mt_address_obj)
{
	mt_struct_base *mt_ptr = NULL;
	if(!parse_pointer(mt_address_obj, (void **)&mt_ptr))
		return NULL;

	// Since the original ctypes.Structure object could be copied or pickled/unpickled,
	// the state vector pointer may be invalid.
	// We are refreshing it using known offset from our mt_struct_base structure.
	mt_ptr->state = &(mt_ptr->state_vec);
	return (mt_struct *)mt_ptr;
}

static uint32_t random_uint32(mt_struct *mt)
{
	return genrand_mt_modified(mt);
}

// Taken from Python standard library
/* random_random is the function named genrand_res53 in the original code;
* generates a random number on [0,1) with 53-bit resolution; note that
* 9007199254740992 == 2**53; I assume they're spelling "/2**53" as
* multiply-by-reciprocal in the (likely vain) hope that the compiler will
* optimize the division away at compile-time. 67108864 is 2**26. In
* effect, a contains 27 random bits shifted left 26, and b fills in the
* lower 26 bits of the 53-bit numerator.
* The orginal code credited Isaku Wada for this algorithm, 2002/01/09.
*/
static double random_float(mt_struct *mt)
{
	uint32_t a = random_uint32(mt) >> 5, b = random_uint32(mt) >> 6;
	return (double)((a * 67108864.0 + b) * (1.0 / 9007199254740992.0));
}

// *******************************************************
// Exported functions
// *******************************************************

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
	PyObject *res = Py_BuildValue("Oii", ptr, count, mts[0]->nn);
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
	int elem_size = 0, count = 0;

	if(!PyArg_ParseTuple(args, "OiOi:fill_mt_structs",
			&mts_address_obj, &elem_size, &ptr_obj, &count))
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
		// mt_struct.i field, since they are filled only on initialization/usage
		memcpy(elem, ptr[i], offsetof(mt_struct, i));
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

	mt_struct *mt_ptr = parse_mt_struct_ptr(mt_address_obj);
	if(NULL == mt_ptr)
		return NULL;

	sgenrand_mt_modified(seed, mt_ptr);

	Py_RETURN_NONE;
}

// Exported function: fill numpy array with random numbers
static PyObject* dcmt_fill_numpy_rand(PyObject *self, PyObject *args)
{
	PyObject *mt_address_obj = NULL;
	PyArrayObject *arr = NULL;

	if(!PyArg_UnpackTuple(args, "fill_numpy_rand", 2, 2, &mt_address_obj, &arr))
		return NULL;

	size_t size = 1;
	for(int i = 0; i < arr->nd; i++)
		size *= arr->dimensions[i];

	mt_struct *mt_ptr = parse_mt_struct_ptr(mt_address_obj);
	if(NULL == mt_ptr)
		return NULL;

	for(int i = 0; i < size; i++)
		((double*)(arr->data))[i] = random_float(mt_ptr);

	Py_RETURN_NONE;
}

static PyObject* dcmt_fill_numpy_randraw(PyObject *self, PyObject *args)
{
	PyObject *mt_address_obj = NULL;
	PyArrayObject *arr = NULL;

	if(!PyArg_UnpackTuple(args, "fill_numpy_randraw", 2, 2, &mt_address_obj, &arr))
		return NULL;

	size_t size = 1;
	for(int i = 0; i < arr->nd; i++)
		size *= arr->dimensions[i];

	mt_struct *mt_ptr = parse_mt_struct_ptr(mt_address_obj);
	if(NULL == mt_ptr)
		return NULL;

	for(int i = 0; i < size; i++)
		((uint32_t*)(arr->data))[i] = random_uint32(mt_ptr);

	Py_RETURN_NONE;
}

static PyObject *dcmt_random(PyObject *self, PyObject *args)
{
	PyObject *mt_address_obj = NULL;

	if(!PyArg_UnpackTuple(args, "random", 1, 1, &mt_address_obj))
		return NULL;

	mt_struct *mt_ptr = parse_mt_struct_ptr(mt_address_obj);
	if(NULL == mt_ptr)
		return NULL;

	return PyFloat_FromDouble(random_float(mt_ptr));
}

// taken from Python standard library
static PyObject *dcmt_getrandbits(PyObject *self, PyObject *args)
{
    int k, i, bytes;
    unsigned long r;
    unsigned char *bytearray;
    PyObject *result;
	PyObject *mt_address_obj = NULL;

    if(!PyArg_ParseTuple(args, "Oi:getrandbits", &mt_address_obj, &k))
        return NULL;

	mt_struct *mt_ptr = parse_mt_struct_ptr(mt_address_obj);
	if(NULL == mt_ptr)
		return NULL;

    if (k <= 0) {
        PyErr_SetString(PyExc_ValueError, "number of bits must be greater than zero");
        return NULL;
    }

    bytes = ((k - 1) / 32 + 1) * 4;
    bytearray = (unsigned char *)PyMem_Malloc(bytes);
    if (bytearray == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    /* Fill-out whole words, byte-by-byte to avoid endianness issues */
    for (i=0 ; i<bytes ; i+=4, k-=32) {
        r = random_uint32(mt_ptr);
        if (k < 32)
            r >>= (32 - k);
        bytearray[i+0] = (unsigned char)r;
        bytearray[i+1] = (unsigned char)(r >> 8);
        bytearray[i+2] = (unsigned char)(r >> 16);
        bytearray[i+3] = (unsigned char)(r >> 24);
    }

    /* little endian order to match bytearray assignment order */
    result = _PyLong_FromByteArray(bytearray, bytes, 1, 0);
    PyMem_Free(bytearray);
    return result;
}

static PyObject *dcmt_jumpahead(PyObject *self, PyObject *args)
{
	long i, j;
	PyObject *iobj;
	PyObject *remobj;
	PyObject *n;
	uint32_t *mt, tmp;
	PyObject *mt_address_obj;

	if(!PyArg_UnpackTuple(args, "jumpahead", 2, 2, &mt_address_obj, &n))
		return NULL;

	mt_struct *mt_ptr = parse_mt_struct_ptr(mt_address_obj);
	if(NULL == mt_ptr)
		return NULL;

	if (!PyInt_Check(n) && !PyLong_Check(n)) {
		PyErr_Format(PyExc_TypeError, "jumpahead requires an "
			"integer, not '%s'", Py_TYPE(n)->tp_name);
		return NULL;
	}

	mt = mt_ptr->state;
	for (i = mt_ptr->nn - 1; i > 1; i--) {
		iobj = PyInt_FromLong(i);
		if (iobj == NULL)
			return NULL;
		remobj = PyNumber_Remainder(n, iobj);
		Py_DECREF(iobj);
		if (remobj == NULL)
			return NULL;
		j = PyInt_AsLong(remobj);
		Py_DECREF(remobj);
		if (j == -1L && PyErr_Occurred())
			return NULL;
		tmp = mt[i];
		mt[i] = mt[j];
		mt[j] = tmp;
	}

	for (i = 0; i < mt_ptr->nn; i++)
		mt[i] += i+1;

	mt_ptr->i = mt_ptr->nn;
	Py_INCREF(Py_None);
	return Py_None;
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

	{"fill_numpy_rand", dcmt_fill_numpy_rand, METH_VARARGS,
		"Fill numpy array with random floating-point numbers"},
	{"fill_numpy_randraw", dcmt_fill_numpy_randraw, METH_VARARGS,
		"Fill numpy array with random integer numbers"},

	{"random", dcmt_random, METH_VARARGS, "Return random floating-point number"},
	{"getrandbits", dcmt_getrandbits, METH_VARARGS, "Return sequence of random bits"},
	{"jumpahead", dcmt_jumpahead, METH_VARARGS, "Change generator state N draws to the future"},

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
