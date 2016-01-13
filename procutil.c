#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "mmu.h"
#include "param.h"
#include "proc.h"

int stdout = 1;
int stderr = 2;

void save(char* name, struct proc p)
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

    if(write(fd, &p, sizeof(p)) == sizeof(p))
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

struct proc load(char* name)
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

    struct proc p;
    if(read(fd, &p, sizeof(p)) == sizeof(p))
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

    return p;
}

int main(int argc, char *argv[])
{
    int pid = getpid();
    printf(stdout, "pid = %d\n", pid);

    struct proc* p_before = malloc(sizeof(struct proc));
    getproc(p_before);
    printf(stdout, "pid = %d\n", p_before->pid);
    save("backup", *p_before);
    printf(stdout, "before: pid = %d\n", p_before->pid);

    struct proc p_after = load("backup");
    printf(stdout, "after: pid = %d\n", p_after.pid);

    exit();
}