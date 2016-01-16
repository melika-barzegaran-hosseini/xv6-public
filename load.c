#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "mmu.h"
#include "param.h"
#include "proc.h"

#include "fs.h"
#include "file.h"
#include "x86.h"

int stdout = 1;
int stderr = 2;

void load(char* name, void* ptr, int size)
{
    printf(stdout, "loading...\n");

    int fd = open(name, O_RDONLY);
    if(fd >= 0)
    {
        printf(stdout, "the file '%s' is opened.\n", name);
    }
    else
    {
        printf(stderr, "error: an error occured while opening the file '%s'.\n", name);
        exit();
    }

    if(read(fd, ptr, size) == size)
    {
        printf(stdout, "the process info is read from the file '%s'.\n", name);
    }
    else
    {
        printf(stderr, "error: an error occured while reading from the file '%s'.\n", name);
        exit();
    }

    close(fd);

    if(unlink(name) >= 0)
    {
        printf(stdout, "the file '%s' is deleted.\n", name);
    }
    else
    {
        printf(stderr, "error: an error occured while unlinking the file '%s'.\n", name);
        exit();
    }
}

int main(int argc, char *argv[])
{
    struct proc* p = (struct proc*) malloc(sizeof(struct proc));
    load("backup", p, sizeof(struct proc));
    p->pgdir = (pde_t*) malloc(sizeof(pde_t) * p->sz/PGSIZE);
//    p->pgdir = (pde_t*) malloc(12 * PGSIZE);
    p->tf = (struct trapframe*) malloc(sizeof(struct trapframe));
    p->context = (struct context*) malloc(sizeof(struct context));
    p->cwd = (struct inode*) malloc(sizeof(struct inode));

    char* mem = (char*) malloc(p->sz);


    //load("pgdir", p->pgdir, sizeof(p->sz));
    load("tf", p->tf, sizeof(struct trapframe));
    load("context", p->context, sizeof(struct context));
    load("pgdir", p->pgdir, sizeof(pde_t) * p->sz/PGSIZE);
    printf(stdout, "injam\n");
    load("mem", mem, p->sz);

    //proc
    printf(stdout, "======================== userspace_after loading: sz, name, kstack =========================\n");
    printf(stdout, "sz = %d\n", p->sz);
    printf(stdout, "name = %s\n", p->name);
    printf(stdout, "kstack = %s\n", p->kstack);
    printf(stdout, "--------------------------------------------------------------------------------------------\n");

    //mem
    printf(stdout, "========================userspace_after loading: mem=========================\n");
    int i;
    for(i = 0; i < p->sz; i += PGSIZE)
    {
        printf(stdout, "page '%d'th = %d\n", i/PGSIZE, *(mem + i));
    }
    printf(stdout, "-------------------------------------------------------------------------------\n");

    //pgdir
    printf(stdout, "========================userspace_after loading: pgdir=========================\n");
    for(i = 0; i < 12 * PGSIZE; i += PGSIZE)
    {
        printf(stdout, "page '%d'th = %d\n", i/PGSIZE, *(p->pgdir + i));
    }
    printf(stdout, "-------------------------------------------------------------------------------\n");

    //tf
    printf(stdout, "======================== userspace_after loading: tf =========================\n");
    printf(stdout, "edi = %d\n", p->tf->edi);
    printf(stdout, "esi = %d\n", p->tf->esi);
    printf(stdout, "ebp = %d\n", p->tf->ebp);
    printf(stdout, "oesp = %d\n", p->tf->oesp);
    printf(stdout, "ebx = %d\n", p->tf->ebx);
    printf(stdout, "edx = %d\n", p->tf->edx);
    printf(stdout, "ecx = %d\n", p->tf->ecx);
    printf(stdout, "eax = %d\n", p->tf->eax);
    printf(stdout, "gs = %d\n", p->tf->gs);
    printf(stdout, "padding1 = %d\n", p->tf->padding1);
    printf(stdout, "fs = %d\n", p->tf->fs);
    printf(stdout, "padding2 = %d\n", p->tf->padding2);
    printf(stdout, "es = %d\n", p->tf->es);
    printf(stdout, "padding3 = %d\n", p->tf->padding3);
    printf(stdout, "ds = %d\n", p->tf->ds);
    printf(stdout, "padding4 = %d\n", p->tf->padding4);
    printf(stdout, "trapno = %d\n", p->tf->trapno);
    printf(stdout, "err = %d\n", p->tf->err);
    printf(stdout, "eip = %d\n", p->tf->eip);
    printf(stdout, "cs = %d\n", p->tf->cs);
    printf(stdout, "padding5 = %d\n", p->tf->padding5);
    printf(stdout, "eflags = %d\n", p->tf->eflags);
    printf(stdout, "esp = %d\n", p->tf->esp);
    printf(stdout, "ss = %d\n", p->tf->ss);
    printf(stdout, "padding6 = %d\n", p->tf->padding6);
    printf(stdout, "------------------------------------------------------------------------------\n");

    //context
    printf(stdout, "======================== userspace_after loading: context =========================\n");
    printf(stdout, "edi = %d\n", p->context->edi);
    printf(stdout, "esi = %d\n", p->context->esi);
    printf(stdout, "ebx = %d\n", p->context->ebx);
    printf(stdout, "ebp = %d\n", p->context->ebp);
    printf(stdout, "eip = %d\n", p->context->eip);
    printf(stdout, "-----------------------------------------------------------------------------------\n");

    exit();
}