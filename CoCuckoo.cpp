#include "CoCuckoo.h"

#include "MurmurHash3.h"
#include "hash.h"

#define __SC_POWER 15;
static const uint64_t INIT_SIZE = 1 << __SC_POWER;
static const uint64_t KICK_THRESHOLD = 100;

using namespace std;

typedef string Key;
typedef uint64_t HashValueType;

struct LockedSubgraphNumbers
{
    int a;
    int b;
};

//******** Function Declare ********
int cocuckooDoubleSize(CocuckooHashTable &table);
bool performKickOut(CocuckooHashTable &table, KeyValueItem item, int, int);
LockedSubgraphNumbers lockTwoSubgraph(CocuckooHashTable &table, HashValueType pos1, HashValueType pos2);
void lockSubgraph(CocuckooHashTable &table, int subgraphNumber);
void unlockSubgraph(CocuckooHashTable &table, int subgraphNumber);
bool atomicAssignSubgraphNumber(CocuckooHashTable &table, HashValueType pos, int subgraphNumber);
void lockFulltableForResizing(CocuckooHashTable &table, bool write);
void unlockFulltableForResizing(CocuckooHashTable &table, bool write);
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
    table->isSubgraphMaximal = (bool *)(calloc(INIT_SIZE, sizeof(bool)));
    table->subgraphIDs = (int *)(calloc(INIT_SIZE, sizeof(int)));
    table->ufsetP = newUFSet(INIT_SIZE);
    table->locks = (spinlock *)calloc(INIT_SIZE, sizeof(spinlock));
    pthread_rwlock_init(&table->lockForResizing, NULL);
    // subgraphID default to -1
    for (int i = 0; i < table->size; i++)
    {
        table->subgraphIDs[i] = -1;
    }

    table->readerCount = 0;
    table->resizing = false;
    

    generate_seeds(table);

    return table;
}

int cocuckooInsert(CocuckooHashTable &table, const DataType &key, const DataType &value, bool needGlobalLock)
{

    if (needGlobalLock) lockFulltableForResizing(table, false);

    int ha, hb;
    auto hashValue = Hash(table, key);
    ha = hashValue.a;
    hb = hashValue.b;
    // printf("Inserting: hash of %s: %u & %u\n", key.c_str(), ha, hb);
    
    // wrap data
    KeyValueItem item = {.occupied = true, .key = key, .value = value};

    UFSet & ufset = *(table.ufsetP);
    bool * isSubgraphMaximal = table.isSubgraphMaximal;

    int subgraphNumberA = find(&ufset,ha);
    int subgraphNumberB = find(&ufset,hb);
    LockedSubgraphNumbers lockedSubgraphNumbers = lockTwoSubgraph(table, ha, hb);

    

    // Update Value
    if (table.data[ha].key == key)
    {
        table.data[ha].value = value;
        unlockSubgraph(table, lockedSubgraphNumbers.a);
        if (lockedSubgraphNumbers.a != lockedSubgraphNumbers.b) 
        {
            unlockSubgraph(table, lockedSubgraphNumbers.b);
        }
        if (needGlobalLock) unlockFulltableForResizing(table, false);
        return 1;
    }
    if (table.data[hb].key == key)
    {
        table.data[hb].value = value;
        unlockSubgraph(table, lockedSubgraphNumbers.a);
        if (lockedSubgraphNumbers.a != lockedSubgraphNumbers.b) 
        {
            unlockSubgraph(table, lockedSubgraphNumbers.b);
        }
        if (needGlobalLock) unlockFulltableForResizing(table, false);
        return 1;
    }

    // TwoEmpty
    if (table.subgraphIDs[ha] == -1 && table.subgraphIDs[hb] == -1)
    {
        // printf("Insert: TwoEmpty\n");

        // modify graph
        lockSubgraph(table, hb);
        int subgraphNumber = hb;
        if (!atomicAssignSubgraphNumber(table, ha, subgraphNumber) || !atomicAssignSubgraphNumber(table, hb, subgraphNumber))
        {
            // restart insert process
            unlockSubgraph(table, hb);
            if (needGlobalLock) unlockFulltableForResizing(table, false);
            cocuckooInsert(table, key, value);
            return 0;
        }

        addReEdge(ufset, hb, ha);
        
        // modified table
        table.data[ha] = item;
        
        // table.isSubgraphMaximal[ha] = false;
        unlockSubgraph(table, hb);
        if (needGlobalLock) unlockFulltableForResizing(table, false);

        return 0;
    }
    

    // OneEmpty
    // If one of the buckets is valid, do insert
    if (table.subgraphIDs[ha] == -1)
    {
        // printf("Insert: OneEmpty\n");
        
        // modify graph
        if (!atomicAssignSubgraphNumber(table, ha, table.subgraphIDs[hb]))
        {
            // restart insert process
            if (needGlobalLock) unlockFulltableForResizing(table, false);
            cocuckooInsert(table, key, value);
            return 0;
        }

        addReEdge(ufset, hb, ha);
        
        //TODO: Atomic?
        table.data[ha] = item;

        if (needGlobalLock) unlockFulltableForResizing(table, false);

        return 0;
    }
    else if (table.subgraphIDs[hb] == -1)
    {
        // printf("Insert: OneEmpty\n");
        
        // modify graph
        if (!atomicAssignSubgraphNumber(table, hb, table.subgraphIDs[ha]))
        {
            // restart insert process
            if (needGlobalLock) unlockFulltableForResizing(table, false);
            cocuckooInsert(table, key, value);
            return 0;
        }

        addReEdge(ufset, ha, hb);
        
        //TODO: Atomic?
        table.data[hb] = item;

        if (needGlobalLock) unlockFulltableForResizing(table, false);

        return 0;
    }

    // ZeroEmpty
    if (isSubgraphMaximal[subgraphNumberA] && isSubgraphMaximal[subgraphNumberB]) {
        printf("Insert fail predicted! Should resize now\n");

        unlockSubgraph(table, lockedSubgraphNumbers.a);
        if (lockedSubgraphNumbers.a != lockedSubgraphNumbers.b) 
        {
            unlockSubgraph(table, lockedSubgraphNumbers.b);
        }

        // unlock read lock before resizing
        if (needGlobalLock) unlockFulltableForResizing(table, false);

        cocuckooDoubleSize(table);
        cocuckooInsert(table, key, value);
        return 0;
    }
    else if (!isSubgraphMaximal[subgraphNumberA] && !isSubgraphMaximal[subgraphNumberB]) {
        // if (table.subgraphIDs[ha] == table.subgraphIDs[hb])
        if (subgraphNumberA == subgraphNumberB)
        {
            // printf("InsertSameNon\n");
            isSubgraphMaximal[subgraphNumberA] = true;
            unlockSubgraph(table, lockedSubgraphNumbers.a);
            performKickOut(table, item, ha, hb);

            if (needGlobalLock) unlockFulltableForResizing(table, false);

            return 0;
        }
        else
        {
            // printf("InsertDiffNonNon\n");
            performKickOut(table, item, ha, hb);
            merge(&ufset, ha, hb);

            table.subgraphIDs[ha] = find(&ufset, ha);
            table.subgraphIDs[hb] = table.subgraphIDs[ha];
            unlockSubgraph(table, lockedSubgraphNumbers.a);
            unlockSubgraph(table, lockedSubgraphNumbers.b);

            if (needGlobalLock) unlockFulltableForResizing(table, false);
            
            return 0;
        }
    }
    else if (isSubgraphMaximal[subgraphNumberA]) {
            // printf("InsertDiffNonMax\n");
            isSubgraphMaximal[subgraphNumberB] = true;
            unlockSubgraph(table, lockedSubgraphNumbers.a);
            unlockSubgraph(table, lockedSubgraphNumbers.b);

            performKickOut(table, item, ha, hb);

            if (needGlobalLock) unlockFulltableForResizing(table, false);

            merge(&ufset, ha, hb);
            return 0;
    }
    else {
            // printf("InsertDiffNonMax\n");
            isSubgraphMaximal[subgraphNumberA] = true;
            unlockSubgraph(table, lockedSubgraphNumbers.a);
            unlockSubgraph(table, lockedSubgraphNumbers.b);

            performKickOut(table, item, hb, ha);

            if (needGlobalLock) unlockFulltableForResizing(table, false);

            merge(&ufset, ha, hb);
            return 0;
    }

    printf("Shouldn't be here!\n");
    if (needGlobalLock) unlockFulltableForResizing(table, false);
    return 0;
}

bool performKickOut(CocuckooHashTable &table, KeyValueItem item, int insertPosition, int backupPosition) {
    KeyValueItem insertItem = item;
    KeyValueItem kickedItem = table.data[insertPosition];

    // Handling re_edge
    UFSet & ufset = *(table.ufsetP);
    addReEdge(ufset, backupPosition, insertPosition);

    for (int i = 0; i < table.size; i++)
    {
        table.data[insertPosition] = insertItem;
        
        auto hashValue = Hash(table, kickedItem.key);
        int alternatePositon = hashValue.a == insertPosition ? hashValue.b : hashValue.a;

        addReEdge(ufset, insertPosition, alternatePositon);
        removeReEdge(ufset, alternatePositon, insertPosition);

        if (!table.data[alternatePositon].occupied)
        {
            // put in and insert successed
            table.data[alternatePositon] = kickedItem;
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
    uint64_t sizeCheck = table.size;

    lockFulltableForResizing(table, true);

    if (table.size != sizeCheck) // Means another thread has just doubled size
    {
        // No need of resizing
        printf("No need of resizing\n");
        unlockFulltableForResizing(table, true);
        return -1;
    }
    

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

    freeUFSet(table.ufsetP);
    table.ufsetP = newUFSet(table.size);

    freeFighter = table.locks;
    table.locks = (spinlock *)calloc(table.size, sizeof(spinlock));
    free(freeFighter);

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
            cocuckooInsert(table, item.key, item.value, false);
        }
    }

    // unlock whole table
    unlockFulltableForResizing(table, true);

    free(oldData);

    return 0;
}

DataType *cocuckooQuery(CocuckooHashTable &table, const DataType &key)
{

    //TODO: lock

    lockFulltableForResizing(table, false);

    int ha, hb;
    auto hashValue = Hash(table, key);
    ha = hashValue.a;
    hb = hashValue.b;

    if (table.data[ha].occupied &&  table.data[ha].key == key)
    {
        unlockFulltableForResizing(table, false);
        return &table.data[ha].value;
    }
    else if (table.data[hb].occupied &&  table.data[hb].key == key)
    {
        unlockFulltableForResizing(table, false);
        return &table.data[hb].value;
    }
    else
    {
        unlockFulltableForResizing(table, false);
        return NULL;
    }
}

int cocuckooRemove(CocuckooHashTable &table, const DataType &key)
{
    lockFulltableForResizing(table, false);

    int ha, hb;
    auto hashValue = Hash(table, key);
    ha = hashValue.a;
    hb = hashValue.b;
    // printf("Removing: hash of %s: %u & %u\n", key.c_str(), ha, hb);
    int pos = -1;

    LockedSubgraphNumbers lockedSubgraphNumbers = lockTwoSubgraph(table, ha, hb);

    if (table.data[ha].occupied && table.data[ha].key == key)
    {
        pos = ha;
    }
    else if (table.data[hb].occupied && table.data[hb].key == key)
    {
        pos = hb;
    }
    else
    {
        unlockSubgraph(table, lockedSubgraphNumbers.a);
        unlockSubgraph(table, lockedSubgraphNumbers.b);
        unlockFulltableForResizing(table, false);
        return -1;
    }

    table.data[pos].occupied = false;

    // Modify graph
    UFSet & ufset = *(table.ufsetP);

    ufset.id[pos] = -1;
    removeReEdge(ufset, pos == ha ? hb : ha, pos);
    if (ufset.re_edge->at(pos).size() == 0) // No other node point to this node
    {
        table.subgraphIDs[pos] = -1;
        unlockSubgraph(table, lockedSubgraphNumbers.a);
        unlockSubgraph(table, lockedSubgraphNumbers.b);
        unlockFulltableForResizing(table, false);
        return 0;
    }

    vector<int> queue;
    queue.push_back(pos);
    int newRoot = pos;

    while (!queue.empty())
    {
        int node = queue.back();
        queue.pop_back();
        const unordered_set<int> & re_edgeSet = ufset.re_edge->at(node);
        for(unordered_set<int>::iterator it = re_edgeSet.begin();it!=re_edgeSet.end();it++)
        {
            queue.push_back(*it);   
        }

        table.subgraphIDs[node] = newRoot;
        if (node != newRoot)
        {
            ufset.id[node] = newRoot;
        }
    }

    unlockSubgraph(table, lockedSubgraphNumbers.a);
    unlockSubgraph(table, lockedSubgraphNumbers.b);
    unlockFulltableForResizing(table, false);
    return 0;
}

spinlock lockForLockingSubgraph;

LockedSubgraphNumbers lockTwoSubgraph(CocuckooHashTable &table, HashValueType pos1, HashValueType pos2) {
    while (1)
    {
        int subgraphNumberA = table.subgraphIDs[find(table.ufsetP, pos1)];
        int subgraphNumberB = table.subgraphIDs[find(table.ufsetP, pos2)];
        if (subgraphNumberA == -1 || subgraphNumberB == -1) // one of the subgraphs is empty, no need of locking
        {
            return {-1, -1};
        }
        else
        {   
            spin_lock(&lockForLockingSubgraph);
            // Lock subgraphs in order to avoid deadlocks ???
            lockSubgraph(table, subgraphNumberA);
            if (subgraphNumberB != subgraphNumberA)
            {
                lockSubgraph(table, subgraphNumberB);
            }
            spin_unlock(&lockForLockingSubgraph);
        }
        // Check
        int asubgraphNumberA = table.subgraphIDs[find(table.ufsetP, pos1)];
        int asubgraphNumberB = table.subgraphIDs[find(table.ufsetP, pos2)];
        if (subgraphNumberA == asubgraphNumberA && subgraphNumberB == asubgraphNumberB)
        {
            return {subgraphNumberA, subgraphNumberB};
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
    // printf("try locking: %d\n", pos);
    if (pos != -1)
    {
        spin_lock(&table.locks[pos]);
    }
    
    // printf("locked: %d\n", pos);
}

void unlockSubgraph(CocuckooHashTable &table, int pos) {
    // printf("try unlocking: %d\n", pos);
    if (pos != -1)
    {
        spin_unlock(&table.locks[pos]);
    }
    // printf("unlocked: %d\n", pos);
}

bool atomicAssignSubgraphNumber(CocuckooHashTable &table, HashValueType pos, int subgraphNumber){
    spin_lock(&table.lockForSubgraphIDs);
    bool result = true;
    if (table.subgraphIDs[pos] != -1) // Means another thread has changed this 
    {
        result = false;
    }
    else
    {
        if (pos != subgraphNumber)
        {
            table.ufsetP->id[pos] = subgraphNumber;
        }
        table.subgraphIDs[pos] = subgraphNumber;
    }
    spin_unlock(&table.lockForSubgraphIDs);
    return result;
}

void lockFulltableForResizing(CocuckooHashTable &table, bool write) {
    if (write)
    {
        // printf("lock full table for resizing, reader: %d\n", table.lockForResizing.__data.__readers);
        // pthread_rwlock_wrlock(&table.lockForResizing);

        spin_lock(&table.writeLock);
    }
    else {
        // printf("lock full table, reader: %d\n", table.lockForResizing.__data.__readers);
        // pthread_rwlock_rdlock(&table.lockForResizing);

        spin_lock(&table.readLock);
        if (++table.readerCount == 1)
        {
            spin_lock(&table.writeLock);
        }
        spin_unlock(&table.readLock);
    }
    
}

void unlockFulltableForResizing(CocuckooHashTable &table, bool write) {
    // printf("unlocked, reader: %d\n", table.lockForResizing.__data.__readers);
    // pthread_rwlock_unlock(&table.lockForResizing);
    if (write)
    {
        spin_unlock(&table.writeLock);
    }
    else {

        spin_lock(&table.readLock);
        if (--table.readerCount == 0)
        {
            spin_unlock(&table.writeLock);
        }
        spin_unlock(&table.readLock);
    }
}
