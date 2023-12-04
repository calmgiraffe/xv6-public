#include "types.h"
#include "user.h"
#include "pstat.h"

int main(void) {
    struct pstat ps;
    getpinfo(&ps);

    for (int i = 0; i < NPROC; i++) {
        if (ps.inuse[i]) {
            printf(1, "tickets: %d; pid: %d; ticks: %d\n", 
            ps.tickets[i], ps.pid[i], ps.ticks[i]);
        }
    }
    exit();
}