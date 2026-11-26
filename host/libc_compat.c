// ****************************************************************************
//
//            Host runtime - libc functions missing in MinGW
//
// ****************************************************************************
// Some games call random() / srandom() (BSD/newlib, not in MinGW CRT).
// Own deterministic generator (does not touch rand() of the CRT): the state
// starts at a fixed value and srandom() sets it. The declarations are in
// host_prelude.h (force-included, also into C++ game code).

#include "host_internal.h"

static unsigned int RandomState = 1;

void srandom(unsigned int seed)
{
	RandomState = seed;
}

// 31-bit result as random() (splitmix32 sequence)
long random(void)
{
	unsigned int z = (RandomState += 0x9E3779B9u);
	z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
	z = (z ^ (z >> 13)) * 0xC2B2AE35u;
	z ^= z >> 16;
	return (long)(z >> 1);
}
