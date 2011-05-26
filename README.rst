This module is a thin Python wrapper over C library `dcmt <http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/DC/dc.html>`_, created by Makoto Matsumoto and Takuji Nishimura, authors of Mersenne twister RNG algorithm.
The original library provides functions for creating multiple independent RNGs (which can be used, for example, in parallel Monte-Carlo simulations).

The package contains intact ``dcmt`` sources (except for ``example`` folder) and exports some functions and structures from its API.
For licensing on original library and other information please see the link above, or ``src/dcmt/README`` file and sources.

The version of the module corresponds to the version of original library, with the addition of the build number.
For example, the wrapper on top of ``dcmt 0.6.1`` may have version like ``0.6.1-10``.
Module documentation can be found `here <http://packages.python.org/dcmt>`_

=========
Changelog
=========

~~~~~~~~~~~~~~~~~~~~~~~~
0.6.1-4 (in development)
~~~~~~~~~~~~~~~~~~~~~~~~

* Fixed bug when mt_range() return 'common' parameters dict
  with non-common parameters included;
* Fixed bug when DcmtRandomState.rand() returned array
  instead of Python float;
* Added raw integer randoms generation to DcmtRandomState;
* Added 'inplace' function to fill provided numpy array with randoms;

* Fix issues with setuptools/distutils (one of them does not support Cython,
  other does not support Sphinx)

~~~~~~~
0.6.1-3
~~~~~~~

* Using slightly optimized random number generation taken from ``numpy`` 1.5.1;
* Module is rewritten using Cython;
* Exporting analogues of Python standard ``random.Random`` and ``numpy``'s
  ``numpy.random.mtrand.RandomState``;
* Renamed ``create_mts`` to ``mt_range``, which now returns dictionary and
  ``numpy`` array instead of ``ctypes`` structures;

~~~~~~~
0.6.1-2
~~~~~~~

* Shortened function names: create_mts(), init_mt()
* Removed get_random() and added rand(), which returns numpy array
* Added create_mts_stripped(), currently as a 'half-documented' feature

~~~~~~~
0.6.1-1
~~~~~~~

* Three basic functions: create_generators(), init_generator() and get_random() (returns integer)
