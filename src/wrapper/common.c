#include <dc.h>
#include <stdbool.h>
#include <stddef.h>

void sgenrand(uint32_t seed, mt_struct *mts);
uint32_t random_uint32(mt_struct *mt);
double random_float(mt_struct *mt);

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

// These two functions are just proxies so that if I ever decide to
// go back to unmodified versions, this is the only part I will have to change

void sgenrand(uint32_t seed, mt_struct *mts)
{
	sgenrand_mt_modified(seed, mts);
}

uint32_t random_uint32(mt_struct *mt)
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
double random_float(mt_struct *mt)
{
	// Theoretically should support any randoms starting from 27 bit and larger
	int ww = mt->ww;
	uint32_t a = random_uint32(mt) >> (ww - 32 + 5), b = random_uint32(mt) >> (ww - 32 + 6);
	return (double)((a * 67108864.0 + b) * (1.0 / 9007199254740992.0));
}
