#include "CoCuckoo.h"

#include "MurmurHash3.h"
#include "hash.h"

#define __SC_POWER 19;
static const uint64_t INIT_SIZE = 1 << __SC_POWER;
static const uint64_t KICK_THRESHOLD = 100;

using namespace std;

typedef string Key;
typedef uint64_t HashValueType;

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
static HashValueType defaultHash(const Key &k, uint64_t seed)
{
    HashValueType h;
    h = cocuckoo_hash(k.c_str(), k.size(), seed);
    return h;
}
static HashValue Hash(CocuckooHashTable &table, const DataType &k)
{
    HashValueType mask = (table.size >> 1) - 1;
    HashValueType value0 = defaultHash(k, table.seeds[0]);
    HashValueType value1 = defaultHash(k, table.seeds[1]);
    HashValue value = {value0 & mask, (value1 & mask) + mask + 1};
    return value;
}

void generate_seeds(CocuckooHashTable * table)
{
    // srand(time(NULL));
    do
    {
        table->seeds[0] = rand();
        table->seeds[1] = rand();
        table->seeds[0] = table->seeds[0] << (rand() % 63);
        table->seeds[1] = table->seeds[1] << (rand() % 63);
    } while (table->seeds[0] == table->seeds[1]);
}
//**************************

//********* Implement ***********
CocuckooHashTable *cocuckooInit()
{

    CocuckooHashTable *table = (CocuckooHashTable *)(calloc(1, sizeof(CocuckooHashTable)));
    table->data = (KeyValueItem *)(calloc(INIT_SIZE, sizeof(KeyValueItem)));
    table->size = INIT_SIZE;
    table->count = 0;
    table->isSubgraphMaximal = (bool *)(calloc(INIT_SIZE, sizeof(bool)));
    table->subgraphIDs = (int *)(calloc(INIT_SIZE, sizeof(int)));
    table->ufsetP = newUFSet(INIT_SIZE);
    table->locks = (spinlock *)calloc(INIT_SIZE, sizeof(spinlock));
    // subgraphID default to -1
    for (int i = 0; i < table->size; i++)
    {
        table->subgraphIDs[i] = -1;
    }
    

    generate_seeds(table);

    return table;
}

int cocuckooInsert(CocuckooHashTable &table, const DataType &key, const DataType &value)
{

    int ha, hb;
    auto hashValue = Hash(table, key);
    ha = hashValue.a;
    hb = hashValue.b;
    // printf("hash of %s: %u & %u\n", key.c_str(), ha, hb);
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
        table.count++;
        
        // table.isSubgraphMaximal[ha] = false;
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
            isSubgraphMaximal[setNumberB] = true;
            unlockSubgraph(table, setNumberA);
            unlockSubgraph(table, setNumberB);
            performKickOut(table, item, ha);
            merge(&ufsetP, ha, hb);
            return 0;
    }
    else {
            // printf("InsertDiffNonMax\n");
            isSubgraphMaximal[setNumberA] = true;
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

    for (int i = 0; i < table.size; i++)
    {
        table.data[insertPosition] = insertItem;

        if (!kickedItem.occupied)
        {
            table.count++;
            return true;
        }
        
        auto hashValue = Hash(table, kickedItem.key);
        int alternatePositon = hashValue.a == insertPosition ? hashValue.b : hashValue.a;

        if (!table.data[alternatePositon].occupied)
        {
            // put in and insert successed
            table.data[alternatePositon] = kickedItem;
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
    printf("Kick Out Failed! Should not reach here!!!\n");
    return false;
}

int cocuckooDoubleSize(CocuckooHashTable &table)
{

    void * freeFighter;
    
    uint64_t oldSize = table.size;
    table.size <<= 1;
    KeyValueItem *newData = (KeyValueItem *)(calloc(table.size, sizeof(KeyValueItem)));
    KeyValueItem *oldData = table.data;
    table.data = newData;

    freeFighter = table.isSubgraphMaximal;
    table.isSubgraphMaximal = (bool *)(calloc(table.size, sizeof(bool)));
    free(freeFighter);

    freeFighter = table.subgraphIDs;
    table.subgraphIDs = (int *)(calloc(table.size, sizeof(int)));
    free(freeFighter);

    freeFighter = table.ufsetP;
    table.ufsetP = newUFSet(table.size);
    free(freeFighter);

    freeFighter = table.locks;
    table.locks = (spinlock *)calloc(table.size, sizeof(spinlock));
    free(freeFighter);

    table.count = 0;

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
    auto hashValue = Hash(table, key);
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
    auto hashValue = Hash(table, key);
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