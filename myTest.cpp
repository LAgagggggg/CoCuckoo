#include "./smartcuckoo.h"

int main(int argc, char const *argv[])
{
    init();

    auto str = "hhhhh";
    insert(str);
    insert("wtf");

    int result = search("hhhhh");
    printf("%d", result);
    
    return 0;
}
