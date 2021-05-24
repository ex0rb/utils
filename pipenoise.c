#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

/*
    pipenoise.c - Damages data streams to test error correction.
*/

#ifndef DEFAULT_PROB
    #define DEFAULT_PROB 0.0005
#endif

#ifndef BUFFER_SIZE
    #define BUFFER_SIZE 4096
#endif

int parse_float(const char *str, float *out)
{
    char *end;

    errno = 0;
    *out = strtof(str, &end);

    return (errno != 0 || end == str);
}

int main(int argc, char *argv[])
{
    char buffer[BUFFER_SIZE];
    const char *write_off;
    ssize_t avail_num, write_num, iter;
    float prob = DEFAULT_PROB;

    srand(time(NULL));

    /* arguments */
    if (argc > 1)
        if (argc != 2 || parse_float(argv[1], &prob) != 0)
        {
            fprintf(stderr, "usage: pipenoise <probability>\n");
            return EXIT_FAILURE;
        }

    for (;;)
    {
        /* read bytes */
        if ((avail_num = read(STDIN_FILENO, buffer, BUFFER_SIZE)) <= 0)
            return (avail_num == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
        else
        {
            /* modify some of them according to prob */
            for (iter = 0; iter < avail_num; iter++)
            {
                if (rand() / (float) RAND_MAX < prob)
                    buffer[iter] += rand() & 0xff;
            }

            /* write out all of them */
            write_off = buffer;
            while (avail_num)
            {
                if ((write_num = write(STDOUT_FILENO, write_off, avail_num)) < 0)
                    return EXIT_FAILURE;
                else if (write_num > 0)
                {
                    avail_num -= write_num;
                    write_off += write_num;
                }
            }
        }
    }
}
