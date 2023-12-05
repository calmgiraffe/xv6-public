#include "random.h"
#include "user.h"

void testProcess(int tickets) {
    settickets(tickets);
    while (1);
}

int main(int argc, char *argv[]) {
    int pid1, pid2, pid3;
    
    if ((pid1 = fork()) == 0) {
        testProcess(20); 
    } else if ((pid2 = fork()) == 0) {
        testProcess(30);
    } else if ((pid3 = fork()) == 0) {
        testProcess(40);
    }
        
    exit();
}