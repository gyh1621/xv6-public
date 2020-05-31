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

  if(argint(0, &pid) < 0)
    return -1;
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

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
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

extern pte_t *walkpgdir(pde_t *, const void *, int);

int sys_wrprotect(void) {
  void *addr;
  int size;

  if (argint(1, &size) < 0|| argptr(0, (void *)&addr, size) < 0)
    return -1;

  char *a, *last;
  pte_t *pte, *pgdir = myproc()->pgdir;

  a = (char*)PGROUNDDOWN((uint)addr);
  last = (char*)PGROUNDDOWN(((uint)addr) + size - 1);

  for(;;){
    if((pte = walkpgdir(pgdir, a, 0)) == 0)
      return -1;
    *pte = (*pte | PTE_W) ^ PTE_W;
    if(a == last)
      break;
    a += PGSIZE;
  }

  // invalidate tlbs
  lcr3(V2P(pgdir));

  //struct file *stdout = myproc()->ofile[1];
  //filewrite(stdout, "entered", 8);
  //cprintf("entered");
  return 0;
}
