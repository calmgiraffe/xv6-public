#include "types.h"
#include "user.h"

#define NULL 0
#define PAGESIZE 4096

void thr_proc1(void *arg1, void *arg2) {
    int *a = (int *) arg1;
    int *b = (int *) arg2;

    printf(1, "arg1: %d\n", *a);
    printf(1, "arg2: %d\n", *b);

    exit();
}

/* Testing/debugging of the features added in kthreads project */
int main(int argc, char *argv[]) {
    int arg1 = 12, arg2 = 34;

    int chd_pid1 = thread_create(thr_proc1, &arg1, &arg2);
    thread_join(chd_pid1);

    exit(); // main() needs to end with this
}