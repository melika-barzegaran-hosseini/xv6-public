#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#include "fs.h"
#include "file.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;

  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));

  pid = np->pid;

  // lock to force the compiler to emit the np->state write last.
  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;

  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot 
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

int getproc(struct proc* p)
{
  p->sz = proc->sz;
  safestrcpy(p->name, proc->name, sizeof(proc->name));
  *(p->tf) = *(proc->tf);
  *(p->context) = *(proc->context);

  cprintf("==================== kernelspace-origin_sz, name, kstack ====================\n");
  cprintf("sz = %d\n", proc->sz);
  cprintf("name = %s\n", proc->name);
  cprintf("kstack = %s\n", proc->kstack);
  cprintf("-----------------------------------------------------------------------------\n");

  cprintf("===================== kernelspace-copy_sz, name, kstack =====================\n");
  cprintf("sz = %d\n", p->sz);
  cprintf("name = %s\n", p->name);
  cprintf("kstack = %s\n", p->kstack);
  cprintf("-----------------------------------------------------------------------------\n");

  cprintf("==================== kernelspace-origin_tf ====================\n");
  cprintf("edi = %d\n", proc->tf->edi);
  cprintf("esi = %d\n", proc->tf->esi);
  cprintf("ebp = %d\n", proc->tf->ebp);
  cprintf("oesp = %d\n", proc->tf->oesp);
  cprintf("ebx = %d\n", proc->tf->ebx);
  cprintf("edx = %d\n", proc->tf->edx);
  cprintf("ecx = %d\n", proc->tf->ecx);
  cprintf("eax = %d\n", proc->tf->eax);
  cprintf("gs = %d\n", proc->tf->gs);
  cprintf("padding1 = %d\n", proc->tf->padding1);
  cprintf("fs = %d\n", proc->tf->fs);
  cprintf("padding2 = %d\n", proc->tf->padding2);
  cprintf("es = %d\n", proc->tf->es);
  cprintf("padding3 = %d\n", proc->tf->padding3);
  cprintf("ds = %d\n", proc->tf->ds);
  cprintf("padding4 = %d\n", proc->tf->padding4);
  cprintf("trapno = %d\n", proc->tf->trapno);
  cprintf("err = %d\n", proc->tf->err);
  cprintf("eip = %d\n", proc->tf->eip);
  cprintf("cs = %d\n", proc->tf->cs);
  cprintf("padding5 = %d\n", proc->tf->padding5);
  cprintf("eflags = %d\n", proc->tf->eflags);
  cprintf("esp = %d\n", proc->tf->esp);
  cprintf("ss = %d\n", proc->tf->ss);
  cprintf("padding6 = %d\n", proc->tf->padding6);
  cprintf("---------------------------------------------------------------\n");

  cprintf("===================== kernelspace-copy_tf =====================\n");
  cprintf("edi = %d\n", p->tf->edi);
  cprintf("esi = %d\n", p->tf->esi);
  cprintf("ebp = %d\n", p->tf->ebp);
  cprintf("oesp = %d\n", p->tf->oesp);
  cprintf("ebx = %d\n", p->tf->ebx);
  cprintf("edx = %d\n", p->tf->edx);
  cprintf("ecx = %d\n", p->tf->ecx);
  cprintf("eax = %d\n", p->tf->eax);
  cprintf("gs = %d\n", p->tf->gs);
  cprintf("padding1 = %d\n", p->tf->padding1);
  cprintf("fs = %d\n", p->tf->fs);
  cprintf("padding2 = %d\n", p->tf->padding2);
  cprintf("es = %d\n", p->tf->es);
  cprintf("padding3 = %d\n", p->tf->padding3);
  cprintf("ds = %d\n", p->tf->ds);
  cprintf("padding4 = %d\n", p->tf->padding4);
  cprintf("trapno = %d\n", p->tf->trapno);
  cprintf("err = %d\n", p->tf->err);
  cprintf("eip = %d\n", p->tf->eip);
  cprintf("cs = %d\n", p->tf->cs);
  cprintf("padding5 = %d\n", p->tf->padding5);
  cprintf("eflags = %d\n", p->tf->eflags);
  cprintf("esp = %d\n", p->tf->esp);
  cprintf("ss = %d\n", p->tf->ss);
  cprintf("padding6 = %d\n", p->tf->padding6);
  cprintf("---------------------------------------------------------------\n");

  cprintf("==================== kernelspace-origin_context ====================\n");
  cprintf("edi = %d\n", proc->context->edi);
  cprintf("esi = %d\n", proc->context->esi);
  cprintf("ebx = %d\n", proc->context->ebx);
  cprintf("ebp = %d\n", proc->context->ebp);
  cprintf("eip = %d\n", proc->context->eip);
  cprintf("--------------------------------------------------------------------\n");

  cprintf("===================== kernelspace-copy_context =====================\n");
  cprintf("edi = %d\n", p->context->edi);
  cprintf("esi = %d\n", p->context->esi);
  cprintf("ebx = %d\n", p->context->ebx);
  cprintf("ebp = %d\n", p->context->ebp);
  cprintf("eip = %d\n", p->context->eip);
  cprintf("--------------------------------------------------------------------\n");

  return 0;
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)p2v(PTE_ADDR(*pde));
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = v2p(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint)va);
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if(*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

int getpgdir(pde_t* pgdir, char *mem)
{
  //pgdir : d ==> output of function copyuvm
//  uint sz = proc->sz;
  pde_t *pte;
  uint pa, i, flags;

    for(i = 0; i < 12 * PGSIZE; i += PGSIZE) {
      if ((pte = walkpgdir(proc->pgdir, (void *) i, 0)) == 0)
        panic("copyuvm: pte should exist");
      if (!(*pte & PTE_P))
        panic("copyuvm: page not present");
      pa = PTE_ADDR(*pte);
      flags = PTE_FLAGS(*pte);
      memmove(mem + i, (char *) p2v(pa), PGSIZE);
      mappages(pgdir, (void *) i, PGSIZE, v2p(mem), flags);
      cprintf("==================== debug ====================\n");
      cprintf("i = %d, pte = %d, base: page address = %d, page value = %d\n", i, pte, mem + i, *(mem + i));
      cprintf("                  copy: page address = %d, page value = %d\n", (char*)p2v(pa), *((char*)p2v(pa)));
    }


  cprintf("==================== kernelspace-origin_pgdir ====================\n");
  for(i = 0; i < 12 * PGSIZE; i += PGSIZE)
  {
    cprintf("page '%d'th = %d\n", i/PGSIZE, *(proc->pgdir + i));
  }
  cprintf("------------------------------------------------------------------\n");

  cprintf("===================== kernelspace-copy_pgdir =====================\n");
  for(i = 0; i < 12 * PGSIZE; i += PGSIZE)
  {
    cprintf("page '%d'th = %d\n", i/PGSIZE, *(mem + i));
  }
  cprintf("------------------------------------------------------------------\n");

  return 0;
}