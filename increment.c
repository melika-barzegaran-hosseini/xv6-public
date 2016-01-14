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

void stop(void)
{
    struct proc* p = malloc(sizeof(struct proc));
    getproc(p);
    save("backup", *p);
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

                if(counter % 5 == 0)
                {
                    stop();
                }
            }

            exit();
        }
    }
}