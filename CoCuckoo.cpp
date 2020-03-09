#include "CoCuckoo.h"

#define __SC_POWER 5;
static const uint32_t MAX_SIZE = 1 << __SC_POWER;
static const uint32_t KICK_THRESHOLD = 10;

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
    
    for (int i = 0; i < 2; i++) {
        seeds[i] = uint32_t(rand());
    }

    return table;
}

int cocuckooInsert(CocuckooHashTable &table, const DataType &key, const DataType &value){

    int ha, hb;
    Hash(key);
    ha = hh[0];
    hb = hh[1];
    // wrap data
    // KeyValueItem item = {.valid = false, .key = key, .value = value };
    KeyValueItem item = {true, key, value};

    // Update Value
    if (table.data[ha].key == value)  
    {
        table.data[ha].value = value;
        return 0;
    }
    if (table.data[hb].key == value)  
    {
        table.data[hb].value = value;
        return 0;
    }

    // If one of the buckets is valid, do insert
    if (!table.data[ha].occupied)
    {
        table.data[ha] = item;
        return 0;
    }
    else if (!table.data[hb].occupied)
    {
        table.data[hb] = item;
        return 0;
    }

    // None of the buckets are valid, do kick out
    KeyValueItem insertItem = item;
    int t = time(NULL)%2;
    int insertPosition = hh[t];
    KeyValueItem kickedItem = table.data[insertPosition];

    for (int i = 0; i < KICK_THRESHOLD; i++)
    {
        table.data[insertPosition] = insertItem;
        Hash(kickedItem.key);
        int alternatePositon = hh[0] == insertPosition ? hh[1] : hh[0];

        if (!table.data[alternatePositon].occupied)
        {
            // put in and insert successed
            table.data[alternatePositon] = kickedItem;
            return 0;
        }
        else {
            insertItem = kickedItem;
            kickedItem = table.data[alternatePositon];
            insertPosition = alternatePositon;
        }
    }

    // Limit exceeded, expand the table
    return 0;

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