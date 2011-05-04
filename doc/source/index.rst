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

 >>> from dcmt import DcmtRandom

Creating three independent RNGs with default parameters:

 >>> rngs = DcmtRandom.range(3, gen_seed=777)

If ``seed`` is not specified, the value from system RNG or timer is taken.
Now we may explicitly initialize each generator
(although they are initialised with random seed by default):

 >>> rngs[0].seed(1)
 >>> rngs[1].seed(2)
 >>> rngs[2].seed(3)

Here seeds can be omitted too.
They are independent from the one that was used to create generators.
After initialisation every generator is ready to produce random numbers:

 >>> rngs[0].random()
 0.23113428363530963
 >>> rngs[1].random()
 0.2925564946366318
 >>> rngs[2].random()
 0.5276839180208412
 >>> rngs[0].random()
 0.3694159212179581
 >>> rngs[1].random()
 0.2539639751553334
 >>> rngs[2].random()
 0.5633233881529953

RNG objects support pickling and copying, so you can save and restore their states freely.

For further information about generator properties and various pitfalls
please refer to the `Reference`_ section.

Mersenne twister RNG parameters
===============================

Before diving into module description it is necessary to clarify the purpose
of different RNG creation parameters, which can be encountered in various constructors.

* ``wordlen`` specifies the range of integer numbers RNG produces during each step:
  it is [0, 2 ** ``wordlen``).
  Larger ranges (like 53-bit float, for example) can be achieved by combining random numbers
  from several subsequent steps of RNG.

  **Supported values:** 31, 32.

* ``exponent`` corresponds to the period of RNG, which equals to 2 ** ``exponent`` - 1.
  After this number of steps RNG will start to repeat previously generated sequence.

  **Supported values:** 521, 607, 1279, 2203, 2281, 3217, 4253, 4423, 9689,
  9941, 11213, 19937, 21701, 23209, 44497.

* ``id`` as a keyword parameter or as an element of range in ``*range`` functions
  stands for RNG identifier.
  It can correspond, for example, to processor ID or thread ID in distributed
  calculations.

  **Supported values:** [0, 65536).

* ``seed``, ``gen_seed``: values for initialising RNGs.
  Any RNG needs two seeds: one to create RNG itself,
  and one to initialise its state (the latter is the common usage of term "seed").
  Along with the parameters described above these two numbers fully specify created RNGs,
  and subsequent calls to the same functions with the same parameters will produce
  exactly the same RNG objects.
  If ``None`` is passed as a seed (which is a default value in all functions),
  the value of the seed will be taken from system RNG or current time.
  If ``int`` or ``long`` is passed, their lowest 4 bytes are taken;
  if the passed object is hashable, the lowest 4 bytes of its hash value are taken;
  otherwise ``DcmtParameterError`` is thrown.

  **Supported values:** [0, 2 ** 32) or ``None``.

.. warning:: There is a known bug in the algorithm where it fails to create RNG
             for ``wordlen=31``, ``exponent=521`` and ``id=9``.
             The function will throw :py:exc:`~DcmtParameterError` if this ID
             belongs to the range of requested IDs.

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

.. class:: DcmtRandom([seed], wordlen=32, exponent=521, id=0, gen_seed=None)

   Class, mimicking ``random.Random`` from Python standard library.
   For the list of available methods see
   `Python reference <http://docs.python.org/library/random.html>`_.

   For the information on keywords see `Mersenne twister RNG parameters`_.

   .. warning:: Unlike Python ``Random``, you must explicitly call ``seed`` method
                at least once before using any object of this class.

   .. py:classmethod:: range([start], stop, wordlen=32, exponent=521, id=0, seed=None)

      Analogue of built-in ``range`` which creates a list with :py:class:`DcmtRandom` objects
      with given parameters and IDs in ``range(start, stop)``.

      .. note:: The result of this function is not identical to several calls to
                :py:class:`DcmtRandom` constructor,
                since this function specifically aims at creating
                independent RNGs with given range of IDs.

.. class:: DcmtRandomState([seed], wordlen=32, exponent=521, id=0, gen_seed=None)

   Class, partially mimicking `numpy.random.RandomState <http://docs.scipy.org/doc/numpy/reference/generated/numpy.random.mtrand.RandomState.html>`_.
   Currently supported: ``rand``, ``get_state`` and ``set_state`` methods
   along with pickling/copying support (which is missing in ``numpy`` class).

   For the information on keywords see `Mersenne twister RNG parameters`_.

   .. warning:: Unlike Python ``Random``, you must explicitly call ``seed`` method
                at least once before using any object of this class.

   .. py:classmethod:: range([start], stop, wordlen=32, exponent=521, id=0, gen_seed=None)

      Analogue of built-in ``range`` which creates a list with :py:class:`DcmtRandomState` objects
      with given parameters and IDs in ``range(start, stop)``.

      .. note:: The result of this function is not identical to several calls to
                :py:class:`DcmtRandomState` constructor,
                since this function specifically aims at creating
                independent RNGs with given range of IDs.

   .. py:classmethod:: from_mt_range(mt_common, mt_unique)

      Creates list of :py:class:`DcmtRandomState` objects from the result of
      :py:func:`mt_range` function.

.. function:: mt_range([start], stop, wordlen=32, exponent=521, gen_seed=None)

   Creates optimized RNG data with no repeating elements.

   :returns: tuple with two elements: dictionary with common parameters for all RNGs,
             and ``numpy`` array with parameters unique for each generator.

   .. note:: This function uses the same creation algorithm as :py:meth:`DcmtRandomState.range`
             and :py:meth:`DcmtRandom.range`.

   .. note:: This function is intended for usage in MT implementations on GPU,
             so the array with unique parameters contains RNG index, which,
             technically, is intialised only after the call to RNG ``seed()`` method.
             The addition of this parameter allows one to employ the returned continous buffer
             in random number generation without rearranging its elements
             (and, as a bonus, makes entries for separate RNGs 16 bytes long).
