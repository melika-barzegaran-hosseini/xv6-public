#include "types.h"
#include "user.h"
#include "mmu.h"
#include "param.h"
#include "proc.h"

int stdout = 1;
int stderr = 2;

int main(int argc, char *argv[])
{
    int pid = getpid();
    printf(stdout, "pid = %d\n", pid);

    struct proc* p = malloc(sizeof(struct proc));
    getproc(p);
    printf(stdout, "pid = %d\n", p->pid);

    exit();
}