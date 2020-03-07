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
    cocuckooInsert(*table, str, "wtf");

    int result = search("hhhhh");
    printf("%d\n", result);

    DataType * r = cocuckooQuery(*table, str);
    cout << *r << endl;

    return 0;
}
