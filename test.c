#include "types.h"
#include "user.h"

#define NULL 0
#define PAGESIZE 4096


void thr_proc(void *arg1, void *arg2) {
    int *a = (int *) arg1;
    int *b = (int *) arg2;

    printf(1, "arg1: %d\n", *a);
    printf(1, "arg2: %d\n", *b);

    exit();
}

/* Testing/debugging of the features added in kthreads project */
int main(int argc, char *argv[]) {
    int arg1 = 111, arg2 = 222;
    void *stack = malloc(PAGESIZE);

    int ret = clone(thr_proc, &arg1, &arg2, ((char*) stack) + PAGESIZE - sizeof(uint));
    
    join(&stack);
    free(stack);
    printf(1, "clone() ret val: %d\n", ret);

    exit(); // main() needs to end with this
}