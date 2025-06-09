#include <stdio.h>  // for fprintf, stderr, feof
#include <string.h> // for strlen, strcmp, strerror, strchr
#include <errno.h>  // for errno
#include <stddef.h> // for size_t

#include "tokenizer.c"
#include "parser.c"
#include "encoder.c"

#define LINE_LEN_MAX 256
#define LINE_ONE_BITS_DECLARATION "bits 16\n"

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Error: invalid number of arguments, expected 2\n");
        fprintf(stderr, "Correct Usage: my-assembler input.asm output\n");
        return 1;
    }

    const char *in_name = argv[1];
    const char *out_name = argv[2];

    size_t len = strlen(in_name);
    if (len >= 4 && strcmp(in_name + len - 4, ".asm") != 0)
    {
        fprintf(stderr, "Error: input file does not end with .asm\n");
        return 1;
    }

    FILE *input = fopen(in_name, "r");
    if (!input)
    {
        fprintf(stderr, "Error with input file '%s': %s\n", in_name, strerror(errno));
        return 1;
    }

    FILE *output = fopen(out_name, "wb");
    if (!output)
    {
        fprintf(stderr, "Error with output file '%s': %s\n", out_name, strerror(errno));
        fclose(input);
        return 1;
    }

    char line[LINE_LEN_MAX];
    size_t lineno = 0;
    while (fgets(line, sizeof(line), input))
    {
        lineno++;

        if (lineno == 1)
        {
            if (strlen(line) != strlen(LINE_ONE_BITS_DECLARATION) || strcmp(line, LINE_ONE_BITS_DECLARATION) != 0)
            {
                fprintf(stderr, "Error: expected declaration 'bits 16' on line 1\n");
                fclose(input);
                fclose(output);
                return 1;
            }
            continue;
        }

        if (!strchr(line, '\n') && !feof(input))
        {
            fprintf(stderr, "Error: line %zu too long (max %d characters)\n", lineno, LINE_LEN_MAX - 2);
            fclose(input);
            fclose(output);
            return 1;
        }

        Token *tokens = NULL;
        size_t token_count = 0;
        int result = tokenize_line(line, lineno, &tokens, &token_count);
        if (result != 0)
        {
            fclose(input);
            fclose(output);
            return 1;
        }
        if (token_count == 0)
            continue;

        Instruction inst;
        result = parse_tokens(tokens, token_count, lineno, &inst);
        if (result != 0)
        {
            fclose(input);
            fclose(output);
            return 1;
        }
        // note: parse_tokens frees the tokens on both success and failure,
        // so I nullify the pointer here to avoid confusion or accidental reuse
        tokens = NULL;

        uint8_t buffer[6]; // max instruction size for 8086 is 6 bytes
        size_t out_size = 0;
        result = encode_instruction(&inst, buffer, &out_size, lineno);
        if (result != 0)
        {
            fclose(input);
            fclose(output);
            return 1;
        }

        fwrite(buffer, 1, out_size, output);
    }

    fclose(input);
    fclose(output);
    return 0;
}