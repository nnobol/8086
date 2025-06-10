#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include "assembler.c"

static double sec_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s foo.asm\n", argv[0]);
        exit(1);
    }

    FILE *f = fopen(argv[1], "r");
    if (!f)
    {
        perror(argv[1]);
        exit(1);
    }
    size_t lines = 0;
    int ch;
    while ((ch = fgetc(f)) != EOF)
        if (ch == '\n')
            ++lines;
    fclose(f);

    double t0 = sec_now();
    assemble_file(argv[1], "/dev/null");
    double t1 = sec_now();

    printf("%.0f lines, %.3f s  ⇒  %.0f lines/s\n",
           (double)lines, t1 - t0, lines / (t1 - t0));
}
