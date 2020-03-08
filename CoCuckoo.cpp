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

CocuckooHashTable * cocuckooInit() {
    CocuckooHashTable * table = (CocuckooHashTable *)(malloc(sizeof(CocuckooHashTable)));
    table->data = (KeyValueItem *)(malloc(MAX_SIZE*sizeof(KeyValueItem)));
    table->size = MAX_SIZE;
    return table;
}

int cocuckooInsert(CocuckooHashTable &table, const DataType &key, const DataType &value){

    int ha, hb;
    Hash(key);
    ha = hh[0];
    hb = hh[1];

    if (table.data[ha].key == value || table.data[hb].key == key )  
    {
        // update value
        return 0;
    }

    else
    {
        table.data[ha].key = key;
        table.data[ha].value = value;
        return 0;
    }

}

DataType * cocuckooQuery(CocuckooHashTable &table, const DataType &key){

    int ha, hb;
    Hash(key);
    ha = hh[0];
    hb = hh[1];

    if (table.data[ha].key == key)
    {
        return &table.data[ha].value;
    }
    else if (table.data[hb].key == key)
    {
        return &table.data[hb].value;
    }
    else
    {
        return NULL;
    }
}