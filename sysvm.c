#include "types.h"
#include "defs.h"

int
sys_mprotect(void) {
    char *p;
    int n;

    // Check that the pointer and the int lies within the process address space.
    if ((argptr(0, &p, sizeof(void *)) < 0) || (argint(1, &n) < 0))
        return -1;

    //cprintf("sys_mprotect entry\n");
    return mprotect((void *) p, n);
}

int
sys_munprotect(void) {
    char *p;
    int n;

    // Check that the pointer and the int lies within the process address space.
    if ((argptr(0, &p, sizeof(void *)) < 0) || (argint(1, &n) < 0))
        return -1;

    //cprintf("sys_munprotect entry\n");
    return munprotect((void *) p, n);
}