#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41

extern pte_t *walkpgdir(pde_t *, const void *, int);

void
printcrashinfo(struct trapframe *tf)
{
  cprintf("eax:0x%x\n", tf->eax);
  cprintf("ebx:0x%x\n", tf->ebx);
  cprintf("ecx:0x%x\n", tf->ecx);
  cprintf("edx:0x%x\n", tf->edx);
  cprintf("esi:0x%x\n", tf->esi);
  cprintf("edi:0x%x\n", tf->edi);
  cprintf("esp:0x%x\n", tf->esp);
  cprintf("ebp:0x%x\n", tf->ebp);
  cprintf("eip:0x%x\n", tf->edi);
  uint returna;
  uint ebp = tf->ebp;
  int i;
  for (i = 0; ; i++) {
    returna = *((uint *)(ebp + 4));
    cprintf("#%d\t0x%x\n", i, returna);
    if (returna == 0xffffffff)
      break;
    ebp = *((uint *)ebp);
  }
}

void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());

    // print crash info: user registers and calling stack
    printcrashinfo(tf);

    // check if it is caused by violation of the page protection
    if (tf->err == 7) {
      uint va = rcr2();
      pte_t *pte = walkpgdir(myproc()->pgdir, (void *)va, 0);
      if (pte != 0 && (*pte & PTE_W) == 0) {
        cprintf("Program is trying to access address 0x%x "
                "in a write protected page at: 0x%x\n",
                va, tf->eip);
        // unprotect the page and continue the program
        *pte = *pte | PTE_W;
        lcr3(V2P(myproc()->pgdir));
        cprintf("Unprotect the page and let program run.\n");
      }
    }
    else
    {
      myproc()->killed = 1;
    }
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
