#include "types.h"
#include "user.h"


uint globalint = 0xabcd;


/* Testing/debugging of the features added in vm-xv6 project */
int main(int argc, char *argv[]) {
    int ret;
    // test2 should be 4 pages.
    // 0x0000 = guard page
    // 0x1000 = text + data
    // 0x2000 = guard page
    // 0x3000 = stack

    // Check stack addr -- &n1 is around 0x1800 or so
    printf(1, "addr of .data var globalint: 0x%x\n", &globalint);
    printf(1, "globalint init val: 0x%x\n", globalint);

    // Unprotect the page with the global vars -- globalint
    if ((ret = munprotect((void *) 0x1000, 1)) < 0)
        printf(1, "munprotect: return val -1\n");
    
    printf(1, "attemping rewrite of global var after munprotect...\n");
    globalint = 0x1234;
    printf(1, "globalint now equals: 0x%x\n", globalint);


    if ((ret = mprotect((void *) 0x1000, 1)) < 0)
        printf(1, "mprotect: return val -1\n");
    
    // Sucessfully results in page fault
    printf(1, "attemping read of global var after mprotect...\n");
    printf(1, "globalint still equals: 0x%x\n", globalint);
    

    if ((ret = munprotect((void *) 0x1000, 1)) < 0)
        printf(1, "munprotect: return val -1\n");

    printf(1, "attemping rewrite of global var after munprotect...\n");
    globalint = 0x4567; // should page fault
    printf(1, "globalint now equals: 0x%x\n", globalint);

    exit();
}