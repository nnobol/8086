#include "assert.h"
#include <stdio.h>  // for fprintf, stderr
#include <stdlib.h> // for free

#include "parser.h"

int main()
{
    const char *line = "";
    Token *tokens = NULL;
    size_t token_count = 0;
    int result = tokenize_line(line, 1, &tokens, &token_count);
    assert(result == 0);
    result = validate_syntax(tokens, token_count, 1);
    if (result != 0)
    {
        for (size_t i = 0; i < token_count; i++)
            free(tokens[i].lexeme);
        free(tokens);
        return 1;
    }

    return 0;
}

static inline int validate_syntax(Token *tokens, size_t token_count, size_t lineno)
{
    if (check_bad_tokens(tokens, token_count, lineno) != 0)
        return 1;
    if (check_mnemonic(tokens, lineno) != 0)
        return 1;
    if (check_brackets(tokens, token_count, lineno) != 0)
        return 1;
    if (check_commas(tokens, token_count, lineno) != 0)
        return 1;

    return 0;
}

static inline int check_bad_tokens(const Token *tokens, size_t token_count, size_t lineno)
{
    for (size_t i = 0; i < token_count; i++)
    {
        if (tokens[i].type == T_BAD)
        {
            fprintf(stderr, "Error: invalid token on line %zu: '%s'\n", lineno, tokens[i].lexeme);
            return 1;
        }
    }
    return 0;
}

static inline int check_mnemonic(const Token *tokens, size_t lineno)
{
    if (tokens[0].type != T_MNEMONIC)
    {
        fprintf(stderr, "Error: invalid first token on line %zu: expected a mnemonic, got '%s'\n", lineno, tokens[0].lexeme);
        return 1;
    }
    return 0;
}

static inline int check_brackets(const Token *tokens, size_t token_count, size_t lineno)
{
    int depth = 0;
    for (size_t i = 0; i < token_count; i++)
    {
        if (tokens[i].type == T_C_BRACK)
        {
            if (depth == 0)
            {
                fprintf(stderr, "Error on line %zu: closing ']' without an opening '['\n", lineno);
                return 1;
            }
            depth--;
        }
        else if (tokens[i].type == T_O_BRACK)
        {
            if (depth == 1)
            {
                fprintf(stderr, "Error on line %zu: nested '[' is not allowed\n", lineno);
                return 1;
            }
            depth++;
        }
    }

    if (depth == 1)
    {
        fprintf(stderr, "Error on line %zu: opening '[' without a matching ']'\n", lineno);
        return 1;
    }
    return 0;
}

static inline int check_commas(const Token *tokens, size_t token_count, size_t lineno)
{
    int commas = 0;
    for (size_t i = 0; i < token_count; i++)
        if (tokens[i].type == T_COMMA)
            commas++;
    if (commas > 1)
    {
        fprintf(stderr, "Error on line %zu: expected exactly one ','\n", lineno);
        return 1;
    }
    return 0;
}