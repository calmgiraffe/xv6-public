#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  // kill() tags the process with the 'killed' field in the struct proc
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;

  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

// The sleep() function will just put a process to sleep" (i.e, make its state 
// SLEEPING so it can't be run by the scheduler) on a channel, which is just an 
// arbitrary integer.
// Also used in xv6 for processes that are waiting on something else to happen,
// for example, waiting for a disk to finish reading
int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;

  acquire(&tickslock);

  ticks0 = ticks; // Get the current val of ticks
  while (ticks - ticks0 < n) { // Loop until elaped ticks is >= to n
    if (myproc()->killed) {
      release(&tickslock);
      return -1;
    } 
    // will release and reacquire the lock so sleeping process doesn't hog the lock
    sleep(&ticks, &tickslock);
  }

  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


/* kthreads project addition */
int
sys_clone(void) {
  cprintf("sys_clone\n");
  char *fcn, *arg1, *arg2, *stack;
  int ret0, ret1, ret2, ret3;

  ret0 = argptr(0, &fcn, sizeof(void *));
  ret1 = argptr(1, &arg1, sizeof(void *));
  ret2 = argptr(2, &arg2, sizeof(void *));
  ret3 = argptr(3, &stack, sizeof(void *));
  
  // All return vals should be 0 if correct
  if (ret0 | ret1 | ret2 | ret3)
    return -1;
  
  return clone((void (*)(void*, void*)) fcn, arg1, arg2, stack);
}

int
sys_join(void) {
  cprintf("sys_join\n");
  char *stackptr;
  
  // User passes in address of ptr in join(), which then goes on the stack
  // The value on stack is then copied to stackptr
  if (argptr(0, &stackptr, sizeof(void *)) < 0)
    return -1;
  
  // stackptr contains the address of the pointer (ptr to ptr)
  return join((void **) stackptr);
}
/* kthreads project addition end */