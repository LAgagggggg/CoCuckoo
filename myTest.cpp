#include "./smartcuckoo.h"
#include "CoCuckoo.h"

using namespace std;

int main(int argc, char const *argv[])
{
    init();

    auto str = "hhhhh";
    insert(str);
    insert("wtf");

    int result = search("hhhhh");
    printf("%d\n", result);

    return 0;
}
