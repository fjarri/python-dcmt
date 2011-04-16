import unittest
from dcmt import create_generators, init_generator, get_random, DcmtError

class TestErrors(unittest.TestCase):

	def testWordlen(self):

		# correct
		for wordlen in (31, 32):
			create_generators(wordlen=wordlen, seed=1)

		# incorrect
		for wordlen in (16, 64):
			self.assertRaises(DcmtError, create_generators, wordlen=wordlen, seed=1)

	def testExponent(self):

		#correct_exponents = [521, 607, 1279, 2203, 2281, 3217, 4253, 4423,
		#	9689, 9941, 11213, 19937, 21701, 23209, 44497]

		# small list for testing purposes, since the whole list takes
		# ages to process
		correct_exponents_reduced = [521, 607]

		# correct
		for exponent in correct_exponents_reduced:
			create_generators(exponent=exponent, seed=1)

		# incorrect
		for exponent in (500, 1000, 50000):
			self.assertRaises(DcmtError, create_generators, exponent=exponent, seed=1)

	def testId(self):

		# error: start_id < max_id
		self.assertRaises(DcmtError, create_generators, start_id=1, max_id=0, seed=1)

		# error: start_id < 0
		self.assertRaises(DcmtError, create_generators, start_id=-1, max_id=2, seed=1)

		# error: start_id > 65536
		self.assertRaises(DcmtError, create_generators, start_id=65537, max_id=65538, seed=1)

		# error: max_id > 65536
		self.assertRaises(DcmtError, create_generators, start_id=65534, max_id=65538, seed=1)

	def testBugId9(self):

		# known bug: cannot find generator for wordlen=31, exponent=521 and id=9
		self.assertRaises(DcmtError, create_generators, wordlen=31, exponent=521,
			start_id=7, max_id=10, seed=1)

	def testSeed(self):

		# since keywords are processed in extension, test that even not mentioning
		# keyword parameter works
		create_generators()

		# correct seeds
		for seed in (None, 0, 1, 2**16, long(2**16), 2**32 - 1, long(2**32 - 1)):
			mts = create_generators(seed=seed)
			init_generator(mts[0], seed=seed)

		# incorrect seeds
		for seed in (-1, -2**32 / 2, -2**32, 2**32, 2 * 2**32, long(2**32)):
			self.assertRaises(DcmtError, create_generators, seed=seed)

			mts = create_generators()
			self.assertRaises(DcmtError, init_generator, mts[0], seed=seed)


class TestBasics(unittest.TestCase):

	def testDifferentIds(self):
		mts = create_generators(start_id=0, max_id=1, seed=10)
		init_generator(mts[0], seed=2)
		init_generator(mts[1], seed=3)

		randoms0 = [get_random(mts[0]) for i in xrange(10)]
		randoms1 = [get_random(mts[1]) for i in xrange(10)]

		for r0, r1 in zip(randoms0, randoms1):
			self.assertNotEqual(r0, r1)

	def testDeterminism(self):
		gen_id = 10
		gen_seed = 100
		init_seed = 300

		random_sets = []
		for i in xrange(2):
			mts = create_generators(start_id=gen_id, max_id=gen_id, seed=gen_seed)
			init_generator(mts[0], seed=init_seed)
			randoms = [get_random(mts[0]) for i in xrange(10)]
			random_sets.append(randoms)

		for r0, r1 in zip(random_sets[0], random_sets[1]):
			self.assertEqual(r0, r1)


if __name__ == '__main__':

	suites = []

	for cls in (TestErrors, TestBasics):
		suites.append(unittest.TestLoader().loadTestsFromTestCase(cls))

	all_tests = unittest.TestSuite(suites)
	unittest.TextTestRunner(verbosity=1).run(all_tests)
