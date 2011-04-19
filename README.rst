This module is a thin Python wrapper over C library `dcmt <http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/DC/dc.html>`_, created by Makoto Matsumoto and Takuji Nishimura, authors of Mersenne twister RNG algorithm.
The original library provides functions for creating multiple independent RNGs (which can be used, for example, in parallel Monte-Carlo simulations).

The package contains intact ``dcmt`` sources (except for ``example`` folder) and exports some functions and structures from its API.
For licensing on original library and other information please see the link above, or ``src/dcmt/README`` file and sources.

The version of the module corresponds to the version of original library, with the addition of the build number.
For example, the wrapper on top of ``dcmt 0.6.1`` may have version like ``0.6.1-10``.

=========
Changelog
=========

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
