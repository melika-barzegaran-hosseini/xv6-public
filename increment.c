#include "types.h"
#include "user.h"

int stdout = 1;
int stderr = 2;

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
            }

            exit();
        }
    }
}