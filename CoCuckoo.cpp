#include "CoCuckoo.h"

#define __SC_POWER 20;
static const uint32_t MAX_SIZE = 1 << __SC_POWER;

static uint32_t seeds[2]; 
static uint32_t hh[2];

using namespace std;

typedef string Key;

static uint32_t defaultHash(const Key &k, uint32_t seed) {
    uint32_t h;
    MurmurHash3_x86_32(k.c_str(), k.size(), seed, &h);
    return h;
}

static void Hash(const DataType &k)
{
	uint32_t mask = MAX_SIZE - 1;
	hh[0] = defaultHash(k, seeds[0]) & mask;
	hh[1] = defaultHash(k, seeds[1]) & mask;void cocuckoo_insert();

}