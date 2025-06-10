#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    if (argc != 5)
    {
        fprintf(stderr,
                "usage: %s <input.asm> <output.asm> <target-lines> <start-line>\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    const char *in_path = argv[1];
    const char *out_path = argv[2];
    long target_lines = strtol(argv[3], NULL, 10);
    long start_line = strtol(argv[4], NULL, 10);

    if (target_lines <= 0 || start_line <= 0)
    {
        fprintf(stderr, "target-lines and start-line must be positive\n");
        return EXIT_FAILURE;
    }

    FILE *in = fopen(in_path, "r");
    if (!in)
        die(in_path);

    char *body = NULL;
    size_t cap = 0;
    size_t len = 0;

    FILE *tmp = open_memstream(&body, &len);
    if (!tmp)
        die("open_memstream");

    FILE *out = fopen(out_path, "w");
    if (!out)
        die(out_path);

    char *line = NULL;
    size_t lcap = 0;
    ssize_t n;
    long lineno = 0;

    while ((n = getline(&line, &lcap, in)) != -1)
    {
        ++lineno;
        if (lineno < start_line)
        {
            if (fwrite(line, 1, n, out) != (size_t)n)
                die("write header");
        }
        else
        {
            if (fwrite(line, 1, n, tmp) != (size_t)n)
                die("memstream");
        }
    }
    fclose(in);
    fclose(tmp);

    long written_lines = lineno < start_line ? lineno : start_line - 1;
    if (written_lines >= target_lines)
        goto done;

    long body_lines = 0;
    for (size_t i = 0; i < len; ++i)
        if (body[i] == '\n')
            ++body_lines;
    if (body_lines == 0)
    {
        fprintf(stderr, "nothing to duplicate (start-line too late?)\n");
        return EXIT_FAILURE;
    }

    while (written_lines < target_lines)
    {
        if (fwrite(body, 1, len, out) != len)
            die("write dup");
        written_lines += body_lines;
    }

done:
    fclose(out);
    free(body);
    free(line);
    return EXIT_SUCCESS;
}
