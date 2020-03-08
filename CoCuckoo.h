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

using namespace std;

typedef std::string DataType;

typedef struct keyvalue_item
{
    DataType key;
    DataType value;
} KeyValueItem;

typedef struct cocuckoo_hashtable
{
    KeyValueItem * data;
    uint32_t size;
} CocuckooHashTable;


CocuckooHashTable * cocuckooInit();
int cocuckooInsert(CocuckooHashTable &table, const DataType &key, const DataType &value);
DataType * cocuckooQuery(CocuckooHashTable &table, const DataType &key);

#endif