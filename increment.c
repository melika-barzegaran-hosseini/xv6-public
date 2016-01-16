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

void save(char* name, void* ptr, int size)
{
    printf(stdout, "saving...\n");

    int fd = open(name, O_CREATE|O_RDWR);
    if(fd >= 0)
    {
        printf(stdout, "the file '%s' is created.\n", name);
    }
    else
    {
        printf(stderr, "error: an error occured while creating the file '%s'.\n", name);
        exit();
    }

    if(write(fd, ptr, size) == size)
    {
        printf(stdout, "the process info is written to the file '%s'.\n", name);
    }
    else
    {
        printf(stderr, "error: an error occured while writing to the file '%s'.\n", name);
        exit();
    }

    close(fd);
}

void stop(void)
{
    struct proc* p = (struct proc*) malloc(sizeof(struct proc));

    p->tf = (struct trapframe*) malloc(sizeof(struct trapframe));
    p->context = (struct context*) malloc(sizeof(struct context));
    p->cwd = (struct inode*) malloc(sizeof(struct inode));

    getproc(p);

    //sz, name, kstack
    printf(stdout, "========================userspace=========================\n");
    printf(stdout, "before saving: sz, name, kstack\n");
    printf(stdout, "==========================================================\n");
    printf(stdout, "sz = %d\n", p->sz);
    printf(stdout, "name = %s\n", p->name);
    printf(stdout, "kstack = %s\n", p->kstack);
    printf(stdout, "----------------------------------------------------------\n");

    //tf
    printf(stdout, "========================userspace=========================\n");
    printf(stdout, "before saving: tf\n");
    printf(stdout, "==========================================================\n");
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
    printf(stdout, "----------------------------------------------------------\n");

    //context
    printf(stdout, "========================userspace=========================\n");
    printf(stdout, "before saving: context\n");
    printf(stdout, "==========================================================\n");
    printf(stdout, "edi = %d\n", p->context->edi);
    printf(stdout, "esi = %d\n", p->context->esi);
    printf(stdout, "ebx = %d\n", p->context->ebx);
    printf(stdout, "ebp = %d\n", p->context->ebp);
    printf(stdout, "eip = %d\n", p->context->eip);
    printf(stdout, "----------------------------------------------------------\n");

    save("backup", p, sizeof(struct proc));
    save("tf", p->tf, sizeof(struct trapframe));
    save("context", p->context, sizeof(struct context));

    int num = p->sz / PGSIZE;
    pde_t* pgdir = (pde_t*) malloc(sizeof(pde_t) * num);
    char* pgs = (char *) malloc(p->sz);
    int i;
    for(i = 0; i < num; i++)
    {
        *(pgdir + i) = (pde_t) (pgs + i * PGSIZE);
    }

    getpgs(pgdir, pgs);

    //pgs
    printf(stdout, "========================userspace=========================\n");
    printf(stdout, "before saving: pgs\n");
    printf(stdout, "==========================================================\n");
    for(i = 0; i < num; i++)
    {
        printf(stdout, "page '%d'th = %d\n", i, *(pgs + i * PGSIZE));
    }
    printf(stdout, "----------------------------------------------------------\n");

    save("pgs", pgs, sizeof(p->sz));

    exit();
}

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        printf(stderr, "usage: increment [integer]\n");
        exit();
    }
    else
    {
        int base = 1;
        int limit = atoi(argv[1]);

        if(limit <= 0)
        {
            printf(stderr, "error: the increment argument must be greater than zero.\n");
            exit();
        }
        else
        {
            printf(stdout, "starting to count...\n");

            int counter;
            for(counter = base; counter <= limit; counter++)
            {
                printf(stdout, "%d\n", counter);
                sleep(100);

                if(counter % 2 == 0)
                {
                    stop();
                }
            }

            exit();
        }
    }
}