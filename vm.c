#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // pointer to global page dir that will replace entrypgdir

// Set up CPU's kernel segment descriptors. Uses an identify map like before.
// Run once on entry on each CPU.
// Sets permission flags for each segment as well as notion of ring levels.
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  // Note: 0xffffffff = 4 GB
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

// Returns a ptr to the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc,
// create any required page tables.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &(pgdir[PDX(va)]); // get the virtual address of the the page directory entry

  if (*pde & PTE_P) { // check if pde (ptr to page table + flags) has present flag set
    pgtab = (pte_t*) P2V(PTE_ADDR(*pde));

  } else {
    // If entry not present and we don't want to alloc, return NULL ptr.
    // If we want to alloc, alloc a page for the new page table, and check return value.
    // If err return value, return NULL ptr.
    if (!alloc || (pgtab = (pte_t*) kalloc()) == 0)
      return 0;

    // Make sure all those PTE_P bits are zero.
    // Undo previous memset() garbage filling
    memset(pgtab, 0, PGSIZE);

    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 otherwise.
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  // Get ptrs to start of first and last pages to map
  a = (char*) PGROUNDDOWN((uint) va);
  last = (char*) PGROUNDDOWN(((uint) va) + size - 1);

  for (;;) {
    // Use walkpgdir() with alloc = 1 to get a ptr to a page table entry.
    // If NULL ptr, return -1 to indicate error
    if ((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;

    // If the returned page has already been allocated, panic. Obviously,
    // something would be seriously wrong with the walkpgdir function
    if (*pte & PTE_P)
      panic("remap");

    // Phys addr, permission gits for PTE, and present flag
    *pte = pa | perm | PTE_P;

    if (a == last)
      break;

    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
 { (void*) KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*) KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*) data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*) DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
  pde_t *pgdir;
  struct kmap *k;

  // Allocate a page of memory for a new page directory table
  if ((pgdir = (pde_t*) kalloc()) == 0)
    return 0;

  memset(pgdir, 0, PGSIZE);

  // Sanity check:
  // PHYSTOP should be below DEVSPACE, which is where I/O devices are defined
  if (P2V(PHYSTOP) > (void*) DEVSPACE)
    panic("PHYSTOP too high");

  // Iterate through kmap[], assign pages to each section
  // kmap: the table that defines the kernel's mappings
  for (k = kmap; k < &kmap[NELEM(kmap)]; k++) {
    // args:
    // mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
    // mappages returns -1 if failure. Therefore on failure, setupkvm returns NULL
    if (mappages(
          pgdir, 
          k->virt, 
          k->phys_end - k->phys_start, 
          (uint) k->phys_start, 
          k->perm) < 0) {
      freevm(pgdir);
      return 0;
    }
  }
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{ 
  // Do some basic sanity checks
  if (p == 0)
    panic("switchuvm: no process");
  if (p->kstack == 0)
    panic("switchuvm: no kstack");
  if (p->pgdir == 0)
    panic("switchuvm: no pgdir");

  // Temporarily disable interrupts
  pushcli();

  // mycpu() returns ptr to struct cpu.
  // struct cpu has a GDT entry. Update this GDT with the same struct's
  // task state ptr and privlege level 0.
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts, sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  // The GDT now points to the task state.
  // Store a segment selector and the stack pointer in the task state.
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
  lcr3(V2P(p->pgdir));  // switch to process's address space

  // Enable interrupts
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;

  if(sz >= PGSIZE) // function only sets up first page
    panic("inituvm: more than a page");

  // Allocate a fresh page of memory and clear it
  mem = kalloc(); 
  memset(mem, 0, PGSIZE);

  // Create a mapping for va 0 using the new page, then move the data from init
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir. addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  uint i, pa, n;
  pte_t *pte;

  // Sanity check: destination va must be page aligned
  if ((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");

  // Debugging
  //cprintf("addr = %x, offset = %x, sz = %x\n\n" , addr, offset, sz);

  // Iterate from addr to addr + sz
  for (i = 0; i < sz; i += PGSIZE) {
    // Get the pte for addr.
    // Another function previously allocates the required pages.
    if ((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");

    pa = PTE_ADDR(*pte);
    if (sz - i < PGSIZE) // If remaining size is less than PGSIZE, change n
      n = sz - i;
    else
      n = PGSIZE;

    if (readi(ip, P2V(pa), offset+i, n) != n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;

  // Sanity check: don't grow into kernel space
  if (newsz >= KERNBASE)
    return 0;

  // Sanity check: newsz should be at least oldsz
  if (newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for (; a < newsz; a += PGSIZE) {
    // Get (virt addr) ptr to new 4K page. Check for NULL ptr, 
    // reverse changes with deallocuvm() if kalloc() fails
    mem = kalloc();
    if (mem == 0) {
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    // Clear the page with 0's
    memset(mem, 0, PGSIZE);

    // Create a mapping in the process pgdir. mem is a ptr to a 4K page. Since 
    // mem is a kernel space address, pa can be easily found with V2P().
    // walkpgdir will return a ptr to a page table entry 'pte'. Then,
    // 'pte' is deferenced and (pa | perm | present flag) written
    if (mappages(pgdir, (char *) a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0) {
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;

  // Sanity check: make sure newsz < oldsz
  if (newsz >= oldsz)
    return oldsz;

  // Get the 'address' of the first page above newsz
  // Note: 'a < oldsz' is due to zero indexing of addresses
  // Recall: page directory has 1024 entries.
  a = PGROUNDUP(newsz);
  for (; a < oldsz; a += PGSIZE) {
    pte = walkpgdir(pgdir, (char *) a, 0);

    // if NULL ptr, page table does not exist, so skip ahead to the next page
    // dir entry. Subtract PGSIZE to cancel the for-loop's update statement.
    if (!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;

    else if ((*pte & PTE_P) != 0) { // check that the pte is actually present
      pa = PTE_ADDR(*pte); // extract the pa; bits 31..12
      if (pa == 0)
        panic("kfree");

      char *v = P2V(pa);
      kfree(v);
      *pte = 0;
    }
  }
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
  uint i;

  if (pgdir == 0)
    panic("freevm: no pgdir");

  deallocuvm(pgdir, KERNBASE, 0); // oldsz = user vm size (KERNBASE)
  for (i = 0; i < NPDENTRIES; i++) {
    // Iterate through page directory entries.
    // If the page directory entry exists, kfree the page for the page table
    // Note: the actual pages for the process (page table entries) are cleared
    // as a result of deallocuvm()
    if (pgdir[i] & PTE_P) {
      char *v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0); // get pte for uva
  if (pte == 0)
    panic("clearpteu");

  *pte &= ~PTE_U; // clear flag
}

// Given a parent process's page directory, create a copy
// of this pgedir for a child process.
pde_t* 
copyuvm(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
  char *mem;

  // Set up kernel part of the new address space.
  // All processes have the same kernel mappings.
  if ((d = setupkvm()) == 0) 
    return 0;

  // Iterate over parent process's address space, 0 to sz.
  // proc.c: copyuvm(curproc->pgdir, curproc->sz)
  // Note: sz is the memory footprint of the process, typically rounded up
  // Note: first page invalid. Starting here better than removing second panic
  for (i = PGSIZE; i < sz; i += PGSIZE) {
    if ((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");

    if (!(*pte & PTE_P))
      panic("copyuvm: page not present");

    // Get the pa and flags from the parent pte.
    // Then, get ptr to a kernel page for the child pte
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if ((mem = kalloc()) == 0) 
      goto bad;

    // Copy bytes from parent to child page.
    // Then, stick this newly alloc'd child page into its own page directory 'd'
    memmove(mem, (char*) P2V(pa), PGSIZE);
    if (mappages(d, (void *) i, PGSIZE, V2P(mem), flags) < 0) {
      kfree(mem);
      goto bad;
    }
  }
  return d;

bad:
  freevm(d);
  return 0;
}

//PAGEBREAK!
// Map user virtual address to kernel address.
// This function only gets called by copyout(), which only gets called by exec().
// exec() gets called by sys_exec(), the shell, and the initial userspace program
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0); // note: no NULL check
  if ((*pte & PTE_P) == 0)
    return 0;
  if ((*pte & PTE_U) == 0)
    return 0;

  return (char*) P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  buf = (char *) p;
  while (len > 0) {
    va0 = (uint) PGROUNDDOWN(va); // get page aligned va
    pa0 = uva2ka(pgdir, (char*) va0); // get kernel virtual address for va

    if (pa0 == 0)
      return -1;

    // length of data is PGSIZE - amount that was pgrounded down
    // if last page, copy len bytes -- len is the amount of B remaining to copy
    n = PGSIZE - (va - va0); 
    if (n > len)
      n = len;

    // copy from buf into target userspace va
    memmove(pa0 + (va - va0), buf, n);

    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

/* Change the protection bits of the page range starting at addr and of len
pages to be read-only. Thus, reads are allowed, but writes should cause a trap
and kill the process. */
int
mprotect(void *addr, int len) {
  // Check that addr is page aligned
  return 0;
}

/* Opposite of mprotect -- set the page range to readable and writable */
int munprotect(void *addr, int len) {
  // Check that addr is page aligned
  return 0;
}