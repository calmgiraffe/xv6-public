#include "types.h"
#include "user.h"

/* Testing/debugging of the features added in vm-xv6 project */
int main(int argc, char *argv[]) {
    printf(1, "text segment = %x\n", &main);

    char *p1 = (char *) atoi(argv[1]); 
    printf(1, "deferencing: %x\n", *p1);

    int pid1;
    if ((pid1 = fork()) == 0) {
        char *p2 = (char *) atoi(argv[2]);
        printf(1, "deferencing: %x\n", *p2);
        
    }
    wait();

    exit();
}