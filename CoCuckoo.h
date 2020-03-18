#ifndef _COCUCKOO_H_
#define _COCUCKOO_H_

#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <string>

#include "union_find.h"
#include "MurmurHash3.h"
#include <inttypes.h>
#include <stdint.h>
#include <vector>

#include <pthread.h>
#include "spinlock.h"

using namespace std;

typedef std::string DataType;

struct KeyValueItem
{
    bool occupied;
    DataType key;
    DataType value;
};

struct CocuckooHashTable
{
    KeyValueItem * data;
    int * subgraphIDs;
    uint32_t size; // Amount of buckets
    uint32_t count; // Buckets used
    UFSet * ufsetP; // Pointer of UFSet
    bool * isSubgraphMaximal;
    spinlock * locks;
    spinlock lockForSubgraphIDs;
};


CocuckooHashTable * cocuckooInit();
int cocuckooInsert(CocuckooHashTable &table, const DataType &key, const DataType &value);
DataType * cocuckooQuery(CocuckooHashTable &table, const DataType &key);
int cocuckooRemove(CocuckooHashTable &table, const DataType &key);

#endif