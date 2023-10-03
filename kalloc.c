// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

// Frees the range of pages that correspond to the very next page after 'vstart'
// and the page that 'vend' is inside. If the page that 'vstart' corresponds to
// has code or data inside of it, it must not be freed. This function has the
// effect of continuously adding to the front of the free list via kfree() calls
void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*) PGROUNDUP((uint) vstart);
  
  for(; p + PGSIZE <= (char *) vend; p += PGSIZE)
    kfree(p);
}
//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  // sanity check: v is page aligned, not in text or data, and less than PHYSTOP
  if ((uint) v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  // The goal is that use-after-free results in a crash rather than a reference
  // to previously kalloc'd code.
  memset(v, 1, PGSIZE);

  // use_lock is a boolean that indicates whether a lock needs to be used
  // At setup, a lock is not needed because we have a single CPU and irq disabled
  if (kmem.use_lock) 
    acquire(&kmem.lock);

  r = (struct run*) v;
  r->next = kmem.freelist;
  kmem.freelist = r;

  if (kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

  if (kmem.use_lock)
    acquire(&kmem.lock);

  // If empty list, r = NULL. Therefore, kalloc will return NULL.
  // This implies all calls to kalloc() should be paired with a NULL ptr check
  r = kmem.freelist;
  if (r)
    kmem.freelist = r->next;

  if (kmem.use_lock)
    release(&kmem.lock);

  return (char*) r;
}

