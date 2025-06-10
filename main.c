#include <stdio.h>

#include "assembler.c"

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Error: invalid number of arguments, expected 2\n");
        fprintf(stderr, "Correct Usage: my-assembler input.asm output\n");
        return 1;
    }

    // const char *in_name = argv[1];

    size_t len = strlen(argv[1]);
    if (len >= 4 && strcmp(argv[1] + len - 4, ".asm") != 0)
    {
        fprintf(stderr, "Error: input file does not end with .asm\n");
        return 1;
    }

    if (assemble_file(argv[1], argv[2]) != 0)
        return 1;

    return 0;
}