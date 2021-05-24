/*
    This is free and unencumbered software released into the public domain.
    
    Anyone is free to copy, modify, publish, use, compile, sell, or
    distribute this software, either in source code form or as a compiled
    binary, for any purpose, commercial or non-commercial, and by any
    means.
    
    In jurisdictions that recognize copyright laws, the author or authors
    of this software dedicate any and all copyright interest in the
    software to the public domain. We make this dedication for the benefit
    of the public at large and to the detriment of our heirs and
    successors. We intend this dedication to be an overt act of
    relinquishment in perpetuity of all present and future rights to this
    software under copyright law.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
    OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
    ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
    OTHER DEALINGS IN THE SOFTWARE.
    
    For more information, please refer to <https://unlicense.org>
*/

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

/*
    pipenoise.c
        Reads a stream from STDIN, damages it by the given (or default)
        probability, then writes it into STDOUT.
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
