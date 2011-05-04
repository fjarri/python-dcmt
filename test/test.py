import unittest
import numpy
import gc
import copy

from dcmt import DcmtParameterError, DcmtError, DcmtRandom, DcmtRandomState, mt_range


def testLimits(randoms, start, stop):
	stop = float(stop)
	start = float(start)
	ideal_mean = (stop + start) / 2
	ideal_var = (((stop - start) / 2) ** 3 / 3 * 2) / (stop - start)

	return numpy.abs(randoms.mean() - ideal_mean) / (stop - start), \
		numpy.abs(randoms.var() - ideal_var) / ideal_var

def getRandomArray(rng, N):
	if isinstance(rng, DcmtRandom):
		return numpy.array([rng.random() for i in xrange(N)])
	else:
		return rng.rand(N)


class TestErrors(unittest.TestCase):

	def testWordlen(self):

		ids = (1,)
		tests = (
			(DcmtRandom, ()),
			(DcmtRandom.range, ids),
			(DcmtRandomState, ()),
			(DcmtRandomState.range, ids),
			(mt_range, ids)
		)

		for func, args in tests:

			# correct
			for wordlen in (31, 32):
				kwds = dict(wordlen=wordlen, gen_seed=1)
				func(*args, **kwds)

			# incorrect
			for wordlen in (16, 64):
				kwds = dict(wordlen=wordlen, gen_seed=1)
				self.assertRaises(DcmtParameterError, func, *args, **kwds)

	def testExponent(self):

		#correct_exponents = [521, 607, 1279, 2203, 2281, 3217, 4253, 4423,
		#	9689, 9941, 11213, 19937, 21701, 23209, 44497]

		# small list for testing purposes, since the whole list takes
		# ages to process
		correct_exponents_reduced = [521, 607]

		ids = (1,)
		tests = (
			(DcmtRandom, ()),
			(DcmtRandom.range, ids),
			(DcmtRandomState, ()),
			(DcmtRandomState.range, ids),
			(mt_range, ids)
		)

		for func, args in tests:

			# correct
			for exponent in correct_exponents_reduced:
				kwds = dict(exponent=exponent, gen_seed=1)
				func(*args, **kwds)

			# incorrect
			for exponent in (500, 1000, 50000):
				kwds = dict(exponent=exponent, gen_seed=1)
				self.assertRaises(DcmtParameterError, func, *args, **kwds)

	def testId(self):

		# valid calls
		DcmtRandom(id=20, gen_seed=1)
		DcmtRandom.range(1, 4, gen_seed=1)
		DcmtRandomState(id=3000, gen_seed=1)
		DcmtRandomState.range(100, 103, gen_seed=1)
		mt_range(4, gen_seed=1)

		# check that range functions behave like Python range()
		for func in (DcmtRandom.range, DcmtRandomState.range, mt_range):
			for args in ((), (1, 2, 3)):
				self.assertRaises(TypeError, func, *args)

			self.assertEqual(func(3, 1), [])

		# error: id < 0
		self.assertRaises(DcmtParameterError, DcmtRandom, id=-1, gen_seed=1)
		self.assertRaises(DcmtParameterError, DcmtRandom.range, -1, 3, gen_seed=1)
		self.assertRaises(DcmtParameterError, DcmtRandomState, id=-1, gen_seed=1)
		self.assertRaises(DcmtParameterError, DcmtRandomState.range, -1, 2, gen_seed=1)
		self.assertRaises(DcmtParameterError, mt_range, -1, 3, gen_seed=1)

		# error: id > 65535
		self.assertRaises(DcmtParameterError, DcmtRandom, id=65536, gen_seed=1)
		self.assertRaises(DcmtParameterError, DcmtRandom.range, 65535, 65537, gen_seed=1)
		self.assertRaises(DcmtParameterError, DcmtRandomState, id=65536, gen_seed=1)
		self.assertRaises(DcmtParameterError, DcmtRandomState.range, 65535, 65537, gen_seed=1)
		self.assertRaises(DcmtParameterError, mt_range, 65534, 65539, gen_seed=1)
		self.assertRaises(DcmtParameterError, mt_range, 65536, 65539, gen_seed=1)

	def testBugId9(self):

		kwds = dict(wordlen=31, exponent=521, gen_seed=1)

		# known bug: cannot find generator for wordlen=31, exponent=521 and id=9
		self.assertRaises(DcmtParameterError, DcmtRandom, id=9, **kwds)
		self.assertRaises(DcmtParameterError, DcmtRandom.range, 8, 11, **kwds)
		self.assertRaises(DcmtParameterError, DcmtRandomState, id=9, **kwds)
		self.assertRaises(DcmtParameterError, DcmtRandomState.range, 8, 11, **kwds)
		self.assertRaises(DcmtParameterError, mt_range, 7, 11, **kwds)

	def testSeed(self):

		correct_seeds = (None, 0, 1, 2**16, long(2**16), 2**32 - 1, long(2**32 - 1),
			-1, -2**32 / 2, -2**32, 2**32, 2 * 2**32, long(2**32), (1, 2, 3))
		incorrect_seeds = ({}, [])

		ids = (1,)
		tests = (
			(DcmtRandom, (), lambda x: x),
			(DcmtRandom.range, ids, lambda x: x[0]),
			(DcmtRandomState, (), lambda x: x),
			(DcmtRandomState.range, ids, lambda x: x[0]),
			(mt_range, ids, None)
		)

		for func, args, get_rng in tests:

			# check that default (random) seed works
			rng = func(*args)

			if get_rng is not None:
				get_rng(rng).seed()

			# correct seeds
			for seed in correct_seeds:
				rng = func(*args, gen_seed=seed)

				if get_rng is not None:
					get_rng(rng).seed(seed)

			# incorrect seeds
			for seed in incorrect_seeds:
				self.assertRaises(DcmtParameterError, func, *args, gen_seed=seed)

				if get_rng is not None:
					rng = get_rng(func(*args, gen_seed=1))
					self.assertRaises(DcmtParameterError, rng.seed, seed)


class TestBasics(unittest.TestCase):

	def testDifferentIds(self):

		for cls in (DcmtRandom, DcmtRandomState):

			rngs = cls.range(2, gen_seed=10)
			rngs[0].seed(2)
			rngs[1].seed(2)

			N = 10
			randoms0 = getRandomArray(rngs[0], N)
			randoms1 = getRandomArray(rngs[1], N)

			self.assert_((randoms0 != randoms1).all())

	def testDeterminism(self):
		gen_id = 10
		seed = 100
		init_seed = 300

		for cls in (DcmtRandom, DcmtRandomState):

			rng0 = cls(id=gen_id, gen_seed=seed)
			rng0.seed(init_seed)
			rng1 = cls(id=gen_id, gen_seed=seed)
			rng1.seed(init_seed)

			randoms0 = getRandomArray(rng0, 10)
			randoms1 = getRandomArray(rng1, 10)

			self.assert_((randoms0 == randoms1).all())

	def testRangeMethod(self):
		gen_id = 10
		seed = 100
		init_seed = 300
		N = 10

		for cls in (DcmtRandom, DcmtRandomState):
			rngs = cls.range(3, gen_seed=seed)

			for rng in rngs:
				rng.seed(init_seed)

			randoms0 = getRandomArray(rngs[0], N)
			randoms1 = getRandomArray(rngs[1], N)
			randoms2 = getRandomArray(rngs[2], N)

			self.assert_((randoms0 != randoms1).all())
			self.assert_((randoms0 != randoms2).all())

	def testRandomsRange(self):

		for cls in (DcmtRandom, DcmtRandomState):
			for wordlen in (31, 32):
				rng = cls(wordlen=wordlen, gen_seed=99)
				rng.seed(999)

				randoms = getRandomArray(rng, 1000)

				diff_mean, diff_var = testLimits(randoms, 0, 1)
				self.assert_(diff_mean < 0.05)
				self.assert_(diff_var < 0.05)

	def testState(self):

		tests = (
			(DcmtRandom, 'getstate', 'setstate'),
			(DcmtRandomState, 'get_state', 'set_state'),
		)

		for cls, getstate, setstate in tests:
			rng = cls(gen_seed=55)
			rng.seed(555)
			N = 10

			state = getattr(rng, getstate)() # getstate/setstate

			rng2 = copy.copy(rng) # copy

			# reference
			randoms0 = getRandomArray(rng, N)

			getattr(rng, setstate)(state)
			randoms1 = getRandomArray(rng, N)

			randoms2 = getRandomArray(rng2, N)

			self.assert_((randoms0 == randoms1).all())
			self.assert_((randoms0 == randoms2).all())


class TestRandom(unittest.TestCase):

	def testGetrandbits(self):
		rng = DcmtRandom(gen_seed=55)
		rng.seed(555)
		stop = 2 ** 64

		randoms = numpy.array([float(rng.randrange(0, stop)) for i in xrange(1000)])

		diff_mean, diff_var = testLimits(randoms, 0, stop)

		self.assert_(diff_mean < 0.05)
		self.assert_(diff_var < 0.05)


class TestRandomState(unittest.TestCase):

	def testRand(self):

		shape = (9, 10, 11)
		rng = DcmtRandomState(gen_seed=900)
		rng.seed(400)

		randoms = rng.rand(*shape)

		self.assert_(randoms.shape == shape)
		diff_mean, diff_var = testLimits(randoms, 0, 1)
		self.assert_(diff_mean < 0.05)
		self.assert_(diff_var < 0.05)

	def testMtRange(self):
		start = 0
		stop = 3
		seed = 100
		init_seeds = [4, 5, 6]
		N = 10

		mt_common, mt_unique = mt_range(start, stop, gen_seed=seed)
		rngs0 = DcmtRandomState.from_mt_range(mt_common, mt_unique)
		rngs1 = DcmtRandomState.range(start, stop, gen_seed=seed)

		for rng0, rng1, seed in zip(rngs0, rngs1, init_seeds):
			rng0.seed(seed)
			rng1.seed(seed)

		for rng0, rng1 in zip(rngs0, rngs1):
			randoms0 = getRandomArray(rng0, N)
			randoms1 = getRandomArray(rng1, N)
			self.assert_((randoms0 == randoms1).all())


def getLineCount(fname):
	f = open(fname)
	count = len(f.read().split('\n'))
	f.close()
	return count

if __name__ == '__main__':

	suites = []

	for cls in (
			TestErrors,
			TestBasics,
			TestRandom,
			TestRandomState,
		):
		suites.append(unittest.TestLoader().loadTestsFromTestCase(cls))

	all_tests = unittest.TestSuite(suites)
	unittest.TextTestRunner(verbosity=1).run(all_tests)

	gc.collect() # additional test for different pointer- and reference- related bugs

	# Temporary measure while I'm optimizing C/Python ratio in .pyx files
	cython_files = [
		'../src/wrapper/pyrandom.c',
		'../src/wrapper/numpyrandom.c'
	]
	for fname in cython_files:
		print "File: " + fname + ": " + str(getLineCount(fname)) + " lines"
