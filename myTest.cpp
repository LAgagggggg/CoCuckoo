#include "./smartcuckoo.h"
#include "CoCuckoo.h"
#include <iostream>

using namespace std;

void printTable(CocuckooHashTable & t);

int main(int argc, char const *argv[])
{
    init();
    CocuckooHashTable * table = cocuckooInit();

    auto str = "hhhhh";
    insert(str);
    insert("wtf");
    cocuckooInsert(*table, "aha0", "wtf0");
    cocuckooInsert(*table, "aha1", "wtf1");
    cocuckooInsert(*table, "aha2", "wtf2");
    cocuckooInsert(*table, "aha3", "wtf3");
    cocuckooInsert(*table, "aha4", "wtf4");
    cocuckooInsert(*table, "aha5", "wtf5");
    cocuckooInsert(*table, "aha6", "wtf6");
    cocuckooInsert(*table, "aha7", "wtf0");
    cocuckooInsert(*table, "aha8", "wtf1");
    cocuckooInsert(*table, "aha9", "wtf2");
    cocuckooInsert(*table, "aha10", "wtf3");
    cocuckooInsert(*table, "aha11", "wtf4");
    cocuckooInsert(*table, "aha12", "wtf5");
    cocuckooInsert(*table, "aha13", "wtf6");
    cocuckooInsert(*table, "aha144444", "wtf0");
    cocuckooInsert(*table, "aha15", "wtf1");
    cocuckooInsert(*table, "aha16", "wtf2");
    cocuckooInsert(*table, "aha17", "wtf3");
    cocuckooInsert(*table, "aha18", "wtf4");
    cocuckooInsert(*table, "aha19", "wtf5");
    cocuckooInsert(*table, "aha20", "wtf6");

    int result = search("hhhhh");
    printf("%d\n", result);

    // DataType * r = cocuckooQuery(*table, str);
    // cout << *r << endl;
    printTable(*table);

    return 0;
}

void printTable(CocuckooHashTable &table) {
    int i = 0;
    while (i<table.size)
    {
        if (i % 5 == 0){
            putchar('\n');
        }
        if (table.data[i].occupied)
        {
            printf("%3d: %8.8s |", i, table.data[i].key.c_str());
        }
        else {
            printf("%3d: %8s |", i, "");
        }
        i++;
    }
}
