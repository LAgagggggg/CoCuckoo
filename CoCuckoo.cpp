#include "CoCuckoo.h"

#define __SC_POWER 5;
static const uint32_t INIT_SIZE = 1 << __SC_POWER;
static const uint32_t KICK_THRESHOLD = 10;

static uint32_t seeds[2];
static uint32_t hh[2];

using namespace std;

typedef string Key;

int cocuckooDoubleSize(CocuckooHashTable &table);

static uint32_t defaultHash(const Key &k, uint32_t seed)
{
    uint32_t h;
    MurmurHash3_x86_32(k.c_str(), k.size(), seed, &h);
    return h;
}

static void Hash(const DataType &k, uint32_t size)
{
    uint32_t mask = size - 1;
    hh[0] = defaultHash(k, seeds[0]) & mask;
    hh[1] = defaultHash(k, seeds[1]) & mask;
    void cocuckoo_insert();
}

CocuckooHashTable *cocuckooInit()
{

    CocuckooHashTable *table = (CocuckooHashTable *)(malloc(sizeof(CocuckooHashTable)));
    table->data = (KeyValueItem *)(malloc(INIT_SIZE * sizeof(KeyValueItem)));
    table->size = INIT_SIZE;
    table->count = 0;

    for (int i = 0; i < 2; i++)
    {
        seeds[i] = uint32_t(rand());
    }

    return table;
}

int cocuckooInsert(CocuckooHashTable &table, const DataType &key, const DataType &value)
{

    int ha, hb;
    Hash(key, table.size);
    ha = hh[0];
    hb = hh[1];
    // wrap data
    KeyValueItem item = {.occupied = true, .key = key, .value = value};

    // Update Value
    if (table.data[ha].key == value)
    {
        table.data[ha].value = value;
        return 1;
    }
    if (table.data[hb].key == value)
    {
        table.data[hb].value = value;
        return 1;
    }

    // If one of the buckets is valid, do insert
    if (!table.data[ha].occupied)
    {
        table.data[ha] = item;
        table.count++;
        return 0;
    }
    else if (!table.data[hb].occupied)
    {
        table.data[hb] = item;
        table.count++;
        return 0;
    }

    // None of the buckets is valid, perform kick out
    KeyValueItem insertItem = item;
    int t = time(NULL) % 2;
    int insertPosition = hh[t];
    KeyValueItem kickedItem = table.data[insertPosition];

    for (int i = 0; i < KICK_THRESHOLD; i++)
    {
        table.data[insertPosition] = insertItem;
        Hash(kickedItem.key, table.size);
        int alternatePositon = hh[0] == insertPosition ? hh[1] : hh[0];

        if (!table.data[alternatePositon].occupied)
        {
            // put in and insert successed
            table.data[alternatePositon] = kickedItem;
            table.count++;
            return 0;
        }
        else
        {
            insertItem = kickedItem;
            kickedItem = table.data[alternatePositon];
            insertPosition = alternatePositon;
        }
    }

    // Limit exceeded, expand the table
    cocuckooDoubleSize(table);
    // Insert the item after resizing
    cocuckooInsert(table, key, value);

    return 0;
}

int cocuckooDoubleSize(CocuckooHashTable &table)
{
    uint32_t oldSize = table.size;
    table.size <<= 1;
    KeyValueItem *newData = (KeyValueItem *)(malloc(table.size * sizeof(KeyValueItem)));
    KeyValueItem *oldData = table.data;
    table.data = newData;
    for (int i = 0; i < oldSize; i++)
    {
        KeyValueItem &item = oldData[i];
        if (item.occupied)
        {
            cocuckooInsert(table, item.key, item.value);
        }
    }
    free(oldData);
}

DataType *cocuckooQuery(CocuckooHashTable &table, const DataType &key)
{

    int ha, hb;
    Hash(key, table.size);
    ha = hh[0];
    hb = hh[1];

    if (table.data[ha].occupied &&  table.data[ha].key == key)
    {
        return &table.data[ha].value;
    }
    else if (table.data[hb].occupied &&  table.data[hb].key == key)
    {
        return &table.data[hb].value;
    }
    else
    {
        return NULL;
    }
}

int cocuckooRemove(CocuckooHashTable &table, const DataType &key)
{
    int ha, hb;
    Hash(key, table.size);
    ha = hh[0];
    hb = hh[1];

    if (table.data[ha].occupied && table.data[ha].key == key)
    {
        table.data[ha].occupied = false;
        return 0;
    }
    else if (table.data[hb].occupied && table.data[hb].key == key)
    {
        table.data[hb].occupied = false;
        return 0;
    }
    else
    {
        return -1;
    }
}