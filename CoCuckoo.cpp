#include "CoCuckoo.h"

typedef string Key;

uint32_t defaultHash(const Key &k, uint32_t seed) {
    uint32_t h;
    MurmurHash3_x86_32(k.c_str(), k.size(), seed, &h);
    return h;
}

void Hash(const DataType &k)
{
	uint32_t mask = MAX_SIZE - 1;
	hh[0] = defaultHash(k, seeds[0]) & mask;
	hh[1] = defaultHash(k, seeds[1]) & mask;
}