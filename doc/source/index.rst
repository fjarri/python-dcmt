Dynamic creation of Mersenne twisters (DCMT)
============================================

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

Introduction
============

This module is a thin Python wrapper over C library `dcmt <http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/DC/dc.html>`_, created by Makoto Matsumoto and Takuji Nishimura, authors of Mersenne twister RNG algorithm.
The original library provides functions for creating multiple independent RNGs (which can be used, for example, in parallel Monte-Carlo simulations).

Quick Start
===========

We will start from importing the module:

 >>> from dcmt import create_mts, init_mt, rand

Creating three independent RNGs with default parameters:

 >>> mts = create_mts(start_id=0, max_id=2, seed=777)

If ``seed`` is not specified, the value of ``time(NULL)`` is taken.
Now we must initialize each generator:

 >>> init_mt(mts[0], seed=1)
 >>> init_mt(mts[1], seed=2)
 >>> init_mt(mts[2], seed=3)

Here seeds can be omitted too, in which case ``time(NULL)`` is used.
These seeds are independent of the one that was used to create generators.
After initialisation every generator is ready to produce random numbers:

 >>> rand(mts[0], (4,))
 array([1906178639, 1943589963, 1600596477,  592883390], dtype=uint32)
 >>> rand(mts[1], (4,))
 array([2586093748,  199032518, 4119501648, 3446361745], dtype=uint32)
 >>> rand(mts[2], (4,))
 array([ 992714192, 1771411355, 1256520595,   93513968], dtype=uint32)
 >>> rand(mts[0], (4,))
 array([3727792825,   15188662,  882751098, 1784690177], dtype=uint32)
 >>> rand(mts[1], (4,))
 array([3218870609,  450586145, 2537490204,  189269991], dtype=uint32)
 >>> rand(mts[2], (4,))
 array([2266385156, 2486337585, 1586629291, 2718290559], dtype=uint32)

Every element of ``mts`` sequence can be copied or saved freely,
in order to keep the state of this RNG.

For further information about generator properties and various pitfalls
please refer to the `Reference`_ section.

Reference
=========

.. module:: dcmt

.. data:: VERSION

   Tuple with integers, containing the module version, for example ``(0, 6, 1, 1)``.
   Here first three numbers specify the version of original dcmt library
   (because this module is very tightly coupled to C implementation),
   and the last number is the actual version of the wrapper.

.. exception:: DcmtError

   This exception is thrown for an internal error of DCMT algorithm
   (usually some failure to create a generator).

.. exception:: DcmtParameterError

   This exception is thrown if parameters specified for creation/initialization
   of MT generators are incorrect.

.. function:: create_mts(wordlen=32, exponent=521, start_id=0, max_id=0, seed=None)

   Creates sequence of generator objects, which are used to produce random numbers.

   :param wordlen: length in bits of integer random numbers RNGs will produce.
          Can be equal to 31 or 32.

   :param exponent: Mersenne exponent, corresponding to the period of created RNGs
          (period will be equal to 2^p-1). Supported values are:
          521, 607, 1279, 2203, 2281, 3217, 4253, 4423, 9689,
          9941, 11213, 19937, 21701, 23209, 44497.

   :param start_id:
   :param max_id: Range of identifiers for generators. Usually these are
          node, processor or thread IDs. All identifiers must be between 0 and 65535,
          and ``start_id`` must be lower than or equal to ``max_id``.

   :param seed: seed for randomizing generator parameters. If not set, current time is used.
          RNGs created with the same ``seed`` and ID are guaranteed to be the same.

   :returns: sequence of generator objects, which should be considered opaque
             (changing their internal data can lead to undefined consequences
             up to the crash of the whole script).
             These objects can be copied, saved and reused freely.

   .. note:: If function fails to create RNG for one of the IDs, it stops and
             returns already created RNGs.
             If it failed to create the first RNG in sequence,
             :py:exc:`~DcmtError` is raised.

   .. warning:: There is a known bug in the algorithm, when it fails to create RNG
                for ``wordlen=31``, ``exponent=521`` and ``ID=9``.
                The function will throw :py:exc:`~DcmtParameterError` if this ID
                belongs to the range of given IDs.

.. function:: create_mts_stripped(wordlen=32, exponent=521, start_id=0, max_id=0, seed=None)

   Takes the same parameters as :py:func:`~create_mts`, but returns optimized structures
   with no repeating elements.

   :returns: tuple with two elements: ``ctypes`` structure with repeating RNG parameters
             and array of ``ctypes`` structures with parameters unique for each RNG.

   .. note:: This function has the same behavior as :py:func:`~create_mts` (see note).

   .. warning:: If you are using these structures, make sure you know what you are doing.

.. function:: init_mt(mt, seed=None)

   Initializes generator state with given seed.

   :param mt: one of generator objects, created with :py:func:`~create_mts`.

   :param seed: seed for randomizing generator state. If not set, current time is used.
          The function is guaranteed to create the same RNG state if given the same seed.

.. function:: rand(mt, shape)

   Produces ``numpy`` array filled with random numbers; range depends on ``wordlen``
   parameter specified during RNG creation with :py:func:`~create_mts`.

   :param mt: one of generator objects, created with :py:func:`~create_mts`
          and initialized with :py:func:`~init_mt` at least once.

   :param shape: shape of resulting array.

   :returns: ``numpy`` array of type ``numpy.uint32`` with shape ``shape``
             filled with random numbers in range [0, 2^wordlen-1]
