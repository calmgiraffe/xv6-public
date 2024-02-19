#include "types.h"
#include "user.h"
#include "ticketlock.h"

#define NULL 0


typedef struct node_t {
	int pid;
	void *stack;
	struct node_t *next;
} Node;

static Node sentinel = {0, NULL, NULL};
static Node *head = &sentinel;


void lock_init(lock_t *lock) {
    lock->ticket = 0;
    lock->turn = 0;
}

void lock_acquire(lock_t *lock) {
    // everyone who wishes to acquire lock gets unique ticket value
    int myturn = fetch_and_add(&lock->ticket, 1);
    while (lock->turn != myturn);
}

void lock_release(lock_t *lock) {
    lock->turn = fetch_and_add(&lock->turn, 1);
}

int thread_create(void (*start_routine)(void *, void *), void *arg1, void *arg2) {
	Node *newNode;
    char *stack;
    int pid;

    stack = malloc(4096);
    pid = clone(start_routine, arg1, arg2, stack + 4096 - sizeof(uint));

    if (pid > 0) {
		// clone() success
        newNode = malloc(sizeof(Node));
		newNode->pid = pid;
		newNode->stack = stack;
		newNode->next = head->next; // Insert after sentinel
		head->next = newNode;
    }

    return pid;
}

int thread_join(int pid) {
    Node *curr = head;
    Node *next = head->next;

    // head always points to sentinel
    // sentinel->next == NULL if no items in list
    while (next != NULL) {
        if (next->pid == pid) {
            curr->next = next->next; // Remove "next"
			
			join(next->stack);
            free(next->stack);
            free(next);
            return 0;
        }
        curr = next;
        next = next->next;
    }

    return -1; // PID not found, indicating failure to remove
}