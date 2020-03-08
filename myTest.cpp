#include "./smartcuckoo.h"
#include "CoCuckoo.h"
#include <iostream>

using namespace std;

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

    int result = search("hhhhh");
    printf("%d\n", result);

    DataType * r = cocuckooQuery(*table, str);
    cout << *r << endl;

    return 0;
}
