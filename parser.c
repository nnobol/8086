#include "assert.h"
#include <stdio.h>  // for fprintf, stderr
#include <stdlib.h> // for free
#include <string.h> // for strcmp
#include <stdint.h> // for int64_t

#include "parser.h"

static inline int validate_syntax(const Token *tokens, size_t token_count, size_t lineno);
static inline int check_bad_tokens(const Token *tokens, size_t token_count, size_t lineno);
static inline int check_mnemonic(const char *m, TokenType type, size_t lineno);
static inline int check_brackets(const Token *tokens, size_t token_count, size_t lineno);
static inline int check_commas(const Token *tokens, size_t token_count, size_t lineno);
static inline int check_size_specifiers(const Token *tokens, size_t token_count, size_t lineno);
static inline int check_numbers(const Token *tokens, size_t token_count, size_t lineno);
static inline int check_sign_symbols(const Token *tokens, size_t token_count, size_t lineno);
static inline int check_registers(const Token *tokens, size_t token_count, size_t lineno);
static inline MnemonicType classify_mnemonic(const char *mnemonic);

int main()
{
    const char *line = "mov ax, bx";
    Token *tokens = NULL;
    size_t token_count = 0;
    size_t lineno = 25;
    int result = tokenize_line(line, lineno, &tokens, &token_count);
    if (result != 0 || token_count == 0)
        return 1;
    Instruction inst;
    result = parse_tokens(tokens, token_count, lineno, &inst);
    if (result != 0)
        return 1;

    return 0;
}

int parse_tokens(Token *tokens, size_t token_count, size_t lineno, Instruction *inst_out)
{
    (void)inst_out;
    int result = validate_syntax(tokens, token_count, lineno);
    if (result != 0)
    {
        for (size_t i = 0; i < token_count; i++)
            free(tokens[i].lexeme);
        free(tokens);
        return 1;
    }

    switch (classify_mnemonic(tokens[0].lexeme))
    {
    case T_MOV:
        printf("got mov\n");
    }

    return 0;
}

static inline int validate_syntax(const Token *tokens, size_t token_count, size_t lineno)
{
    if (check_bad_tokens(tokens, token_count, lineno) != 0)
        return 1;
    if (check_mnemonic(tokens[0].lexeme, tokens[0].type, lineno) != 0)
        return 1;
    if (check_brackets(tokens, token_count, lineno) != 0)
        return 1;
    if (check_commas(tokens, token_count, lineno) != 0)
        return 1;
    if (check_size_specifiers(tokens, token_count, lineno) != 0)
        return 1;
    if (check_numbers(tokens, token_count, lineno) != 0)
        return 1;
    if (check_sign_symbols(tokens, token_count, lineno) != 0)
        return 1;
    if (check_registers(tokens, token_count, lineno) != 0)
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

static inline int check_mnemonic(const char *m, TokenType type, size_t lineno)
{
    if (type != T_MNEMONIC)
    {
        fprintf(stderr, "Error: invalid first token on line %zu: expected a mnemonic, got '%s'\n", lineno, m);
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
            if (tokens[i + 1].type == T_C_BRACK)
            {
                fprintf(stderr, "Error on line %zu: empty memory operand\n", lineno);
                return 1;
            }

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
    if (tokens[token_count - 1].type == T_COMMA || tokens[1].type == T_COMMA)
    {
        fprintf(stderr, "Error on line %zu: ',' not allowed at that position\n", lineno);
        return 1;
    }

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

static inline int check_size_specifiers(const Token *tokens, size_t token_count, size_t lineno)
{
    if (tokens[token_count - 1].type == T_SIZE)
    {
        fprintf(stderr, "Error on line %zu: size specifier '%s' not allowed as the last token\n", lineno, tokens[token_count - 1].lexeme);
        return 1;
    }

    size_t occurances = 0, first_i = 0, second_i = 0, comma_i = 0;
    for (size_t i = 0; i < token_count; i++)
    {
        if (tokens[i].type == T_COMMA)
            comma_i = i;

        if (tokens[i].type == T_SIZE)
        {
            if (occurances == 0)
                first_i = i;
            else if (occurances == 1)
                second_i = i;

            occurances++;
        }
    }

    if (occurances >= 1)
    {
        if ((comma_i - 1) == first_i)
        {
            fprintf(stderr, "Error on line %zu: cannot write a size specifier before a ','\n", lineno);
            return 1;
        }

        if (occurances == 2)
            if ((comma_i - 1) == second_i)
            {
                fprintf(stderr, "Error on line %zu: cannot write a size specifier before a ','\n", lineno);
                return 1;
            }
    }

    if (occurances > 2)
    {
        fprintf(stderr, "Error on line %zu: more than 2 size specifiers found in the line\n", lineno);
        return 1;
    }

    if (occurances == 2)
    {
        if (strcmp(tokens[first_i].lexeme, tokens[second_i].lexeme) != 0)
        {
            fprintf(stderr, "Error on line %zu: size specifiers of different sizes is not allowed\n", lineno);
            return 1;
        }
    }

    return 0;
}

static inline int check_numbers(const Token *tokens, size_t token_count, size_t lineno)
{
    size_t n_count_outside = 0, n_count_inside = 0, sign_count_inside = 0;
    for (size_t i = 0; i < token_count; i++)
    {
        if (tokens[i].type == T_NUMBER)
        {
            n_count_outside++;
            continue;
        }

        if (tokens[i].type == T_O_BRACK)
        {
            while (tokens[i].type != T_C_BRACK)
            {
                if (tokens[i].type == T_NUMBER)
                {
                    n_count_inside++;
                    i++;
                    continue;
                }

                if (tokens[i].type == T_PLUS || tokens[i].type == T_MINUS)
                {
                    sign_count_inside++;
                    i++;
                    continue;
                }

                i++;
            }
        }
    }

    if (n_count_outside > 1)
    {
        fprintf(stderr, "Error on line %zu: two numbers not allowed\n", lineno);
        return 1;
    }

    if (n_count_outside == 1)
    {
        if (tokens[token_count - 1].type != T_NUMBER)
        {
            fprintf(stderr, "Error on line %zu: number not allowed at that position\n", lineno);
            return 1;
        }
    }

    if (((int64_t)n_count_inside - (int64_t)sign_count_inside) > 1)
    {
        fprintf(stderr, "Error on line %zu: the number of sign symbols and numbers don't match\n", lineno);
        return 1;
    }

    return 0;
}

static inline int check_sign_symbols(const Token *tokens, size_t token_count, size_t lineno)
{
    if (tokens[1].type == T_PLUS || tokens[1].type == T_MINUS)
    {
        fprintf(stderr, "Error on line %zu: sign symbols not allowed at that position\n", lineno);
        return 1;
    }

    for (size_t i = 0; i < token_count; i++)
    {
        if (tokens[i].type == T_PLUS || tokens[i].type == T_MINUS)
            if (tokens[i + 1].type != T_NUMBER)
            {
                fprintf(stderr, "Error on line %zu: sign symbols outside of the memory operand must be followed by a number\n", lineno);
                return 1;
            }

        if (tokens[i].type == T_O_BRACK)
        {
            while (tokens[i].type != T_C_BRACK)
            {
                if (tokens[i].type == T_MINUS)
                {
                    if (tokens[i + 1].type != T_NUMBER)
                    {
                        fprintf(stderr, "Error on line %zu: the minus sign symbol inside of the memory operand must be followed by a number\n", lineno);
                        return 1;
                    }
                }

                if (tokens[i].type == T_PLUS)
                {
                    if (tokens[i - 1].type == T_O_BRACK && tokens[i + 1].type == T_REG)
                    {
                        fprintf(stderr, "Error on line %zu: the plus sign symbol inside of the memory operand not allowed for the first regiser\n", lineno);
                        return 1;
                    }

                    if (tokens[i + 1].type != T_NUMBER && tokens[i + 1].type != T_REG)
                    {
                        fprintf(stderr, "Error on line %zu: the plus sign symbol inside of the memory operand must be followed by a register or a number\n", lineno);
                        return 1;
                    }
                }

                i++;
            }
        }
    }

    return 0;
}

static inline int check_registers(const Token *tokens, size_t token_count, size_t lineno)
{
    size_t reg_count_outside = 0, reg_count_inside = 0, occurances_outside = 0, first_i_outside = 0, occurances_inside = 0, first_i_inside = 0;
    for (size_t i = 0; i < token_count; i++)
    {
        if (tokens[i].type == T_REG)
        {
            if (occurances_outside == 0)
            {
                first_i_outside = i;
                occurances_outside++;
            }
            reg_count_outside++;
            continue;
        }

        if (tokens[i].type == T_O_BRACK)
        {
            while (tokens[i].type != T_C_BRACK)
            {
                if (tokens[i].type == T_REG)
                {
                    if (occurances_inside == 0)
                    {
                        first_i_inside = i;
                        occurances_inside++;
                    }
                    reg_count_inside++;
                    i++;
                    continue;
                }

                i++;
            }
        }
    }

    if ((reg_count_outside + reg_count_inside) > 3)
    {
        fprintf(stderr, "Error on line %zu: too many registers on the line\n", lineno);
        return 1;
    }

    if (reg_count_outside >= 1)
    {
        if (reg_count_outside > 2)
        {
            fprintf(stderr, "Error on line %zu: too many registers on the line\n", lineno);
            return 1;
        }

        if (tokens[first_i_outside + 1].type != T_COMMA)
        {
            fprintf(stderr, "Error on line %zu: the first register operand must be followed by a ','\n", lineno);
            return 1;
        }

        if (reg_count_outside == 2)
        {
            if (reg_count_inside != 0)
            {
                fprintf(stderr, "Error on line %zu: too many registers on the line\n", lineno);
                return 1;
            }

            if (tokens[token_count - 1].type != T_REG)
            {
                fprintf(stderr, "Error on line %zu: the second register operand must the last token\n", lineno);
                return 1;
            }
        }
    }

    if (reg_count_inside >= 1)
    {
        if (reg_count_inside > 2)
        {
            fprintf(stderr, "Error on line %zu: too many registers on the line\n", lineno);
            return 1;
        }

        if (reg_count_inside == 2)
        {
            if (tokens[first_i_inside + 1].type != T_PLUS)
            {
                fprintf(stderr, "Error on line %zu: the first register inside the memory operand must be followed by a '+'\n", lineno);
                return 1;
            }
        }
    }

    return 0;
}

static inline MnemonicType classify_mnemonic(const char *m)
{
    if (strcmp("mov", m) == 0)
        return T_MOV;

    fprintf(stderr, "Internal error: unhandled mnemonic '%s'\n", m);
    exit(2);
}