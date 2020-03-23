#include "./smartcuckoo.h"
#include "CoCuckoo.h"
#include <iostream>

using namespace std;

void printTable(CocuckooHashTable & t);

int main(int argc, char const *argv[])
{
    init();
    CocuckooHashTable &table = *cocuckooInit();

    auto str = "hhhhh";
    insert(str);
    insert("wtf");
    
    //insert into CoCuckoo
    int insertNumber = 100;

    for (int i = 0; i < insertNumber; i++)
    {
        string s = "test";
        s += to_string(i);        
        cocuckooInsert(table, s, "value");
    }
    for (int i = 0; i < insertNumber; i+=3)
    {
        string s = "test";
        s += to_string(i);        
        cocuckooRemove(table, s);
    }
    for (int i = insertNumber; i < 2 * insertNumber; i++)
    {
        string s = "test";
        s += to_string(i);        
        cocuckooInsert(table, s, "value");
    }

    //check
    for (int i = 0; i < 2 * insertNumber; i++)
    {
        string s = "test";
        s += to_string(i);
        auto result = cocuckooQuery(table, s);
        if (result == NULL) {
            printf("\"%s\" not exist\n", s.c_str());
        }
    }

    // int result = search("hhhhh");
    // printf("%d\n", result);

    // DataType * r = cocuckooQuery(*table, str);
    // cout << *r << endl;

    // printTable(table);

    return 0;
}

void printTable(CocuckooHashTable &table) {
    int i = 0;
    while (i<table.size)
    {
        if (i % 8 == 0){
            putchar('\n');
        }
        if (table.data[i].occupied)
        {
            printf("%3d: %10.10s |", i, table.data[i].key.c_str());
        }
        else {
            printf("%3d: %10s |", i, "");
        }
        i++;
    }
    putchar('\n');
}
