#include "CoCuckoo.h"

#define __SC_POWER 5;
static const uint32_t INIT_SIZE = 1 << __SC_POWER;
static const uint32_t KICK_THRESHOLD = 10;

static uint32_t seeds[2];

using namespace std;

typedef string Key;
typedef uint32_t HashValueType;

//******** Function Declare ********
int cocuckooDoubleSize(CocuckooHashTable &table);
bool performKickOut(CocuckooHashTable &table, KeyValueItem item, int positon);
void lockTwoSubgraph(CocuckooHashTable &table, HashValueType pos1, HashValueType pos2);
void lockSubgraph(CocuckooHashTable &table, int subgraphNumber);
void unlockSubgraph(CocuckooHashTable &table, int subgraphNumber);
void atomicAssignSubgraphNumber(CocuckooHashTable &table, HashValueType pos, int subgraphNumber);
//**********************************

//******** Hash Func ********
struct HashValue
{
    HashValueType a;
    HashValueType b;
};
static HashValueType defaultHash(const Key &k, uint32_t seed)
{
    HashValueType h;
    MurmurHash3_x86_32(k.c_str(), k.size(), seed, &h);
    return h;
}
static HashValue Hash(const DataType &k, uint32_t size)
{
    HashValueType mask = size - 1;
    HashValue value = {defaultHash(k, seeds[0]) & mask, defaultHash(k, seeds[1]) & mask};
    return value;
}
//**************************

//********* Implement ***********
CocuckooHashTable *cocuckooInit()
{

    CocuckooHashTable *table = (CocuckooHashTable *)(malloc(sizeof(CocuckooHashTable)));
    table->data = (KeyValueItem *)(malloc(INIT_SIZE * sizeof(KeyValueItem)));
    table->size = INIT_SIZE;
    table->count = 0;
    table->isSubgraphMaximal = (bool *)(malloc(INIT_SIZE * sizeof(bool)));
    table->subgraphIDs = (int *)(malloc(INIT_SIZE * sizeof(int)));
    table->ufsetP = newUFSet(INIT_SIZE);
    table->locks = (spinlock *)calloc(INIT_SIZE, sizeof(spinlock));
    // subgraphID default to -1
    for (int i = 0; i < table->size; i++)
    {
        table->subgraphIDs[i] = -1;
    }
    

    for (int i = 0; i < 2; i++)
    {
        seeds[i] = uint32_t(rand());
    }

    return table;
}

int cocuckooInsert(CocuckooHashTable &table, const DataType &key, const DataType &value)
{

    int ha, hb;
    auto hashValue = Hash(key, table.size);
    ha = hashValue.a;
    hb = hashValue.b;
    printf("hash of %s: %u & %u\n", key.c_str(), ha, hb);
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

    lockTwoSubgraph(table, ha, hb);

    UFSet & ufsetP = *(table.ufsetP);
    bool * isSubgraphMaximal = table.isSubgraphMaximal;
    // TwoEmpty
    if (table.subgraphIDs[ha] == -1 && table.subgraphIDs[hb] == -1)
    {
        // printf("Insert: TwoEmpty\n");

        // modify graph
        ufsetP.id[ha] = hb;
        int subgraphNumber = find(&ufsetP, ha);
        atomicAssignSubgraphNumber(table, ha, subgraphNumber);
        atomicAssignSubgraphNumber(table, hb, subgraphNumber);

        // modified table
        table.data[ha] = item;
        table.data[ha].occupied = true;
        table.count++;
        
        table.isSubgraphMaximal[ha] = false;
        unlockSubgraph(table, subgraphNumber);

        return 0;
    }
    

    // OneEmpty
    // If one of the buckets is valid, do insert
    if (table.subgraphIDs[ha] == -1)
    {
        // printf("Insert: OneEmpty\n");

        // modify graph
        ufsetP.id[ha] = hb;
        atomicAssignSubgraphNumber(table, ha, table.subgraphIDs[hb]);

        //TODO: Atomic?
        table.data[ha] = item;
        table.data[ha].occupied = true;
        table.count++;

        return 0;
    }
    else if (table.subgraphIDs[hb] == -1)
    {
        // printf("Insert: OneEmpty\n");

        // modify graph
        ufsetP.id[hb] = ha;
        atomicAssignSubgraphNumber(table, hb, table.subgraphIDs[ha]);

        //TODO: Atomic?
        table.data[hb] = item;
        table.data[hb].occupied = true;
        table.count++;

        return 0;
    }

    // ZeroEmpty
    int setNumberA = find(&ufsetP,ha);
    int setNumberB = find(&ufsetP,hb);
    if (isSubgraphMaximal[setNumberA] && isSubgraphMaximal[setNumberB]) {
        printf("Insert fail predicted! Should resize now\n");

        unlockSubgraph(table, setNumberA);
        if (setNumberA != setNumberB) 
        {
            unlockSubgraph(table, setNumberB);
        }
        
        cocuckooDoubleSize(table);
        cocuckooInsert(table, key, value);
        return 0;
    }
    else if (!isSubgraphMaximal[setNumberA] && !isSubgraphMaximal[setNumberB]) {
        // if (table.subgraphIDs[ha] == table.subgraphIDs[hb])
        if (setNumberA == setNumberB)
        {
            // printf("InsertSameNon\n");
            isSubgraphMaximal[setNumberA] = true;
            unlockSubgraph(table, setNumberA);
            performKickOut(table, item, ha);
            return 0;
        }
        else
        {
            // printf("InsertDiffNonNon\n");
            performKickOut(table, item, ha);
            merge(&ufsetP, ha, hb);
            table.subgraphIDs[ha] = find(&ufsetP, ha);
            table.subgraphIDs[hb] = table.subgraphIDs[ha];
            unlockSubgraph(table, table.subgraphIDs[ha]);
            unlockSubgraph(table, table.subgraphIDs[hb]);
            return 0;
        }
    }
    else if (isSubgraphMaximal[setNumberA]) {
            // printf("InsertDiffNonMax\n");
            isSubgraphMaximal[table.subgraphIDs[ha]] == true;
            unlockSubgraph(table, setNumberA);
            unlockSubgraph(table, setNumberB);
            performKickOut(table, item, ha);
            merge(&ufsetP, ha, hb);
            return 0;
    }
    else {
            // printf("InsertDiffNonMax\n");
            isSubgraphMaximal[table.subgraphIDs[hb]] == true;
            unlockSubgraph(table, setNumberA);
            unlockSubgraph(table, setNumberB);
            performKickOut(table, item, hb);
            merge(&ufsetP, ha, hb);
            return 0;
    }

    return 0;
}

bool performKickOut(CocuckooHashTable &table, KeyValueItem item, int insertPosition) {
    KeyValueItem insertItem = item;
    KeyValueItem kickedItem = table.data[insertPosition];

    while (1)
    {
        table.data[insertPosition] = insertItem;

        if (!kickedItem.occupied)
        {
            table.count++;
            return true;
        }
        
        auto hashValue = Hash(kickedItem.key, table.size);
        int alternatePositon = hashValue.a == insertPosition ? hashValue.b : hashValue.a;

        if (!table.data[alternatePositon].occupied)
        {
            // put in and insert successed
            table.data[alternatePositon] = kickedItem;
            table.data[alternatePositon].occupied = true;
            table.count++;
            return true;
        }
        else
        {
            insertItem = kickedItem;
            kickedItem = table.data[alternatePositon];
            insertPosition = alternatePositon;
        }
    }
}

int cocuckooDoubleSize(CocuckooHashTable &table)
{
    uint32_t oldSize = table.size;
    table.size <<= 1;
    KeyValueItem *newData = (KeyValueItem *)(malloc(table.size * sizeof(KeyValueItem)));
    KeyValueItem *oldData = table.data;
    table.data = newData;
    table.isSubgraphMaximal = (bool *)(malloc(table.size * sizeof(bool)));
    table.subgraphIDs = (int *)(malloc(table.size * sizeof(int)));
    table.ufsetP = newUFSet(table.size);
    table.locks = (spinlock *)calloc(table.size, sizeof(spinlock));
    // subgraphID default to -1
    for (int i = 0; i < table.size; i++)
    {
        table.subgraphIDs[i] = -1;
    }

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
    auto hashValue = Hash(key, table.size);
    ha = hashValue.a;
    hb = hashValue.b;

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
    auto hashValue = Hash(key, table.size);
    ha = hashValue.a;
    hb = hashValue.b;

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

void lockTwoSubgraph(CocuckooHashTable &table, HashValueType pos1, HashValueType pos2) {
    while (1)
    {
        int subgraphNumberA = table.subgraphIDs[find(table.ufsetP, pos1)];
        int subgraphNumberB = table.subgraphIDs[find(table.ufsetP, pos2)];
        if (subgraphNumberB < subgraphNumberA)
        {
            swap(subgraphNumberA, subgraphNumberB);
        }
        if (subgraphNumberA == -1) // one of the subgraphs is empty, no need of locking
        {
            return;
        }
        else
        {
            // Lock subgraphs in order to avoid deadlocks
            lockSubgraph(table, subgraphNumberA);
            if (subgraphNumberB != subgraphNumberA)
            {
                lockSubgraph(table, subgraphNumberB);
            }
        }
        // Check contigeous
        int asubgraphNumberA = table.subgraphIDs[find(table.ufsetP, pos1)];
        int asubgraphNumberB = table.subgraphIDs[find(table.ufsetP, pos2)];
        if (asubgraphNumberB < asubgraphNumberA)
        {
            swap(asubgraphNumberA, asubgraphNumberB);
        }
        if (subgraphNumberA == asubgraphNumberA && subgraphNumberB == asubgraphNumberB)
        {
            break;
        }
        else
        {
            unlockSubgraph(table, subgraphNumberA);
            if (subgraphNumberB != subgraphNumberA)
            {
                unlockSubgraph(table, subgraphNumberB);
            }
        }
    }
    
}

void lockSubgraph(CocuckooHashTable &table, int pos) {
    spin_lock(&table.locks[pos]);
}

void unlockSubgraph(CocuckooHashTable &table, int pos) {
    spin_unlock(&table.locks[pos]);
}

void atomicAssignSubgraphNumber(CocuckooHashTable &table, HashValueType pos, int subgraphNumber){
    spin_lock(&table.lockForSubgraphIDs);
    table.subgraphIDs[pos] = subgraphNumber;
    spin_unlock(&table.lockForSubgraphIDs);
}