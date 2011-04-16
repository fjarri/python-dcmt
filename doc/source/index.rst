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

 >>> from dcmt import create_generators, init_generator, get_random

Creating three independent RNGs with default parameters:

 >>> mtss = create_generators(start_id=0, max_id=2, seed=777)

If ``seed`` is not specified, the value of ``time(NULL)`` is taken.
Now we must initialize each generator:

 >>> init_generator(mtss[0], seed=1)
 >>> init_generator(mtss[1], seed=2)
 >>> init_generator(mtss[2], seed=3)

Here seeds can be omitted too, in which case ``time(NULL)`` is used.
These seeds are independent of the one that was used to create generators.
After initialisation every generator is ready to produce random numbers:

 >>> get_random(mtss[0])
 1906178639
 >>> get_random(mtss[1])
 2586093748
 >>> get_random(mtss[2])
 992714192
 >>> get_random(mtss[0])
 1943589963
 >>> get_random(mtss[1])
 199032518
 >>> get_random(mtss[2])
 1771411355

Every element of ``mts`` sequence can be copied or saved,
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

   This exception is thrown for any error of DCMT algorithm (invalid parameters,
   failure to create generator etc.).

.. function:: create_generators(wordlen=32, exponent=521, start_id=0, max_id=0, seed=None)

   Creates sequence of generator objects, which are used to produce random numbers.

   :param wordlen: length in bits of integer random numbers RNGs will produce.
          Can be equal to 31 or 32.

   :param exponent: Mersenne exponent, corresponding to the period of created RNGs
          (period will be equal to 2^p-1). Supported values are:
          521, 607, 1279, 2203, 2281, 3217, 4253, 4423, 9689,
          9941, 11213, 19937, 21701, 23209, 44497.

   :param start_id, max_id: Range of identifiers for generators. Usually these are
          node, processor or thread IDs. All identifiers must be between 0 and 65535,
          and ``start_id`` must be lower than or equal to ``max_id``.

   :param seed: seed for randomizing generator parameters. If not set, current time is used.
          RNGs created with the same ``seed`` and ID are guaranteed to be the same.

   :returns: sequence of generator objects, which should be considered opaque
             (changing their internal data can lead to undefined consequences
             up to the crash of the whole script).
             These objects can be copied, saved and reused freely.

   .. warning:: There is a known bug in the algorithm, when it fails to create RNG
                for ``wordlen=31``, ``exponent=521`` and ``ID=9``.
                The function will throw :py:exc:`~DcmtError` if this ID
                belongs to the range of given IDs.

.. function:: init_generator(mts, seed=None)

   Initializes generator state with given seed.

   :param mts: one of generator objects, created with :py:func:`~create_generators`.

   :param seed: seed for randomizing generator state. If not set, current time is used.
          The function is guaranteed to create the same RNG state if given the same seed.

.. function:: get_random(mts)

   Produces integer random number with the range specified by ``wordlen`` parameter
   during RNG creation with :py:func:`~create_generators`.

   :param mts: one of generator objects, created with :py:func:`~create_generators`
          and initialized with :py:func:`~init_generator` at least once.

   :returns: integer random number in range [0, 2^wordlen-1]
