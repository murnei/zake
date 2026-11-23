#include "zrn.h"
#include "stdio.h"

ZRN_ENTRY
void main() {
    char du[14] = "Hello world!";
    print("%s", &du);
    return;
}
