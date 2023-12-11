#include "types.h"
#include "user.h"


/* Testing/debugging of the features added in vm-xv6 project */
int main(int argc, char *argv[]) {
    int ret1 = mprotect((void *) 0, 0);
    int ret2 = munprotect((void *) 0, 0);

    printf(1, "%d, %d\n", ret1, ret2);
        
    exit();
}