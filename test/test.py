import unittest
import numpy
import gc
from dcmt import create_mts, create_mts_stripped, init_mt, rand, DcmtParameterError

class TestErrors(unittest.TestCase):

	def testWordlen(self):

		# correct
		for wordlen in (31, 32):
			create_mts(wordlen=wordlen, seed=1)

		# incorrect
		for wordlen in (16, 64):
			self.assertRaises(DcmtParameterError, create_mts, wordlen=wordlen, seed=1)

	def testExponent(self):

		#correct_exponents = [521, 607, 1279, 2203, 2281, 3217, 4253, 4423,
		#	9689, 9941, 11213, 19937, 21701, 23209, 44497]

		# small list for testing purposes, since the whole list takes
		# ages to process
		correct_exponents_reduced = [521, 607]

		# correct
		for exponent in correct_exponents_reduced:
			create_mts(exponent=exponent, seed=1)

		# incorrect
		for exponent in (500, 1000, 50000):
			self.assertRaises(DcmtParameterError, create_mts, exponent=exponent, seed=1)

	def testId(self):

		# error: start_id < max_id
		self.assertRaises(DcmtParameterError, create_mts, start_id=1, max_id=0, seed=1)

		# error: start_id < 0
		self.assertRaises(DcmtParameterError, create_mts, start_id=-1, max_id=2, seed=1)

		# error: start_id > 65536
		self.assertRaises(DcmtParameterError, create_mts, start_id=65537, max_id=65538, seed=1)

		# error: max_id > 65536
		self.assertRaises(DcmtParameterError, create_mts, start_id=65534, max_id=65538, seed=1)

	def testBugId9(self):

		# known bug: cannot find generator for wordlen=31, exponent=521 and id=9
		self.assertRaises(DcmtParameterError, create_mts, wordlen=31, exponent=521,
			start_id=7, max_id=10, seed=1)

	def testSeed(self):

		# check that default (random) seed works
		create_mts()

		# correct seeds
		for seed in (None, 0, 1, 2**16, long(2**16), 2**32 - 1, long(2**32 - 1),
				-1, -2**32 / 2, -2**32, 2**32, 2 * 2**32, long(2**32),
				(1, 2, 3)):
			mts = create_mts(seed=seed)
			init_mt(mts[0], seed=seed)

		# incorrect seeds
		for seed in ({}, []):
			self.assertRaises(DcmtParameterError, create_mts, seed=seed)

			mts = create_mts()
			self.assertRaises(DcmtParameterError, init_mt, mts[0], seed=seed)


class TestBasics(unittest.TestCase):

	def testDifferentIds(self):
		mts = create_mts(start_id=0, max_id=1, seed=10)
		init_mt(mts[0], seed=2)
		init_mt(mts[1], seed=3)

		shape = (10,)
		randoms0 = rand(mts[0], shape)
		randoms1 = rand(mts[1], shape)

		for r0, r1 in zip(randoms0, randoms1):
			self.assertNotEqual(r0, r1)

	def testDeterminism(self):
		gen_id = 10
		gen_seed = 100
		init_seed = 300

		random_sets = []
		for i in xrange(2):
			mts = create_mts(start_id=gen_id, max_id=gen_id, seed=gen_seed)
			init_mt(mts[0], seed=init_seed)
			randoms = rand(mts[0], (10,))
			random_sets.append(randoms)

		for r0, r1 in zip(random_sets[0], random_sets[1]):
			self.assertEqual(r0, r1)

	def testRange(self):

		for wordlen in (31, 32):
			mts = create_mts(wordlen=wordlen, seed=99)
			init_mt(mts[0])

			r = rand(mts[0], (64,))
			self.assert_((r >= 0).all() and (r <= 2**wordlen - 1).all())


class TestStripped(unittest.TestCase):

	def testValidity(self):
		"""
		Check that common parameters are really common,
		and unique parameters are filled properly
		"""

		params = dict(start_id=0, max_id=2, seed=123)
		mts1 = create_mts(**params)
		mts2_common, mts2_stripped = create_mts_stripped(**params)

		for mt1, mt2 in zip(mts1, mts2_stripped):

			# check common parameters
			for field in (x[0] for x in mts2_common._fields_):
				self.assertEqual(getattr(mt1, field), getattr(mts2_common, field))

			# check unique parameters
			for field in (x[0] for x in mt2._fields_):
				if field != 'i':
					self.assertEqual(getattr(mt1, field), getattr(mt2, field))


if __name__ == '__main__':

	suites = []

	for cls in (TestErrors, TestBasics, TestStripped):
		suites.append(unittest.TestLoader().loadTestsFromTestCase(cls))

	all_tests = unittest.TestSuite(suites)
	unittest.TextTestRunner(verbosity=1).run(all_tests)

	gc.collect() # additional test for different pointer- and reference- related bugs
