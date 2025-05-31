#include "assert.h"
#include <stdio.h>  // for fprintf, stderr
#include <stdlib.h> // for free
#include <string.h> // for strcmp
#include <stdint.h> // for int64_t
#include <stdbool.h>

#include "parser.h"

static inline int validate_syntax(const Token *tokens, size_t token_count, size_t lineno);
static inline MnemonicType classify_mnemonic(const char *mnemonic);

// int main()
// {
//     const char *line = "mov ax, bx";
//     Token *tokens = NULL;
//     size_t token_count = 0;
//     size_t lineno = 25;
//     int result = tokenize_line(line, lineno, &tokens, &token_count);
//     if (result != 0 || token_count == 0)
//         return 1;
//     Instruction inst;
//     result = parse_tokens(tokens, token_count, lineno, &inst);
//     if (result != 0)
//         return 1;

//     return 0;
// }

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

// parser_test.c for details
static inline int validate_syntax(const Token *tokens, size_t token_count, size_t lineno)
{
    for (size_t i = 0; i < token_count; i++)
    {
        if (tokens[i].type == T_BAD)
        {
            fprintf(stderr, "Error on line %zu: invalid token '%s'\n", lineno, tokens[i].lexeme);
            return 1;
        }
    }

    if (tokens[0].type != T_MNEMONIC)
    {
        fprintf(stderr, "Error on line %zu: first token should be a valid mnemonic\n", lineno);
        return 1;
    }

    int64_t bracket_depth = 0;
    size_t mem_op_start = 0;
    size_t mnems = 0, commas = 0, mem_ops = 0, imm_ops = 0, reg_ops = 0, regs_in = 0;
    for (size_t i = 0; i < token_count; i++)
    {
        TokenType prev = (i > 0 ? tokens[i - 1].type : T_INVALID);
        TokenType next = (i + 1 < token_count ? tokens[i + 1].type : T_EOF);

        switch (tokens[i].type)
        {
        case T_MNEMONIC:
            mnems++;
            break;
        case T_COMMA:
            if (next == T_EOF)
            {
                fprintf(stderr, "Error on line %zu: unexpected end of input after ','\n", lineno);
                return 1;
            }

            if (bracket_depth > 0)
            {
                fprintf(stderr, "Error on line %zu: ',' not allowed inside the memory operand\n", lineno);
                return 1;
            }

            if (commas == 1)
            {
                fprintf(stderr, "Error on line %zu: expected exactly one ','\n", lineno);
                return 1;
            }

            if (!(prev == T_REG || prev == T_C_BRACK || prev == T_NUMBER))
            {
                fprintf(stderr, "Error on line %zu: ',' must be between two operands\n", lineno);
                return 1;
            }
            commas++;
            break;
        case T_O_BRACK:
            if (bracket_depth == 1)
            {
                fprintf(stderr, "Error on line %zu: nested '[' is not allowed\n", lineno);
                return 1;
            }
            mem_op_start = i;
            bracket_depth++;
            break;
        case T_C_BRACK:
            if (bracket_depth == 0)
            {
                fprintf(stderr, "Error on line %zu: closing ']' without an opening '['\n", lineno);
                return 1;
            }
            bracket_depth--;
            mem_ops++;
            break;
        case T_SIZE:
            if (next == T_EOF)
            {
                fprintf(stderr, "Error on line %zu: unexpected end of input after '%s'\n", lineno, tokens[i].lexeme);
                return 1;
            }

            if (bracket_depth > 0)
            {
                fprintf(stderr, "Error on line %zu: size specifier not allowed inside the memory operand\n", lineno);
                return 1;
            }
            if (next != T_NUMBER && next != T_REG)
            {
                fprintf(stderr, "Error on line %zu: size specifier must be followed by an immediate or a register\n", lineno);
                return 1;
            }
            break;
        case T_NUMBER:
            if (bracket_depth > 0)
            {
                if (next != T_C_BRACK && next != T_PLUS && next != T_MINUS)
                {
                    fprintf(stderr, "Error on line %zu: number inside memory operand must be followed by '+' or '-' or closing ']'\n", lineno);
                    return 1;
                }
            }
            else
            {
                imm_ops++;
            }
            break;
        case T_PLUS:
        case T_MINUS:
            if (bracket_depth > 0)
            {
                if (tokens[i].type == T_MINUS && next != T_NUMBER)
                {
                    fprintf(stderr, "Error on line %zu: '-' symbol inside the memory operand must be followed by a number\n", lineno);
                    return 1;
                }
                else if (tokens[i].type == T_PLUS && next != T_NUMBER && next != T_REG)
                {
                    fprintf(stderr, "Error on line %zu: '+' symbol inside the memory operand must be followed by a number or a register\n", lineno);
                    return 1;
                }
            }
            else
            {
                if (next != T_NUMBER)
                {
                    fprintf(stderr, "Error on line %zu: sign symbols outside the memory operand must be followed by a number\n", lineno);
                    return 1;
                }
            }
            break;
        case T_REG:
            if (bracket_depth > 0)
            {
                regs_in++;
            }
            else
            {
                reg_ops++;
            }
            break;
        default:
            break;
        }
    }

    if (mnems > 1)
    {
        fprintf(stderr, "Error on line %zu: expected exactly one mnemonic\n", lineno);
        return 1;
    }

    if (bracket_depth == 1)
    {
        fprintf(stderr, "Error on line %zu: opening '[' without a matching ']'\n", lineno);
        return 1;
    }

    size_t operand_count = mem_ops + reg_ops + imm_ops;
    if (operand_count > 2)
    {
        fprintf(stderr, "Error on line %zu: too many operands (maximum 2 allowed)\n", lineno);
        return 1;
    }

    if (operand_count == 2 && commas != 1)
    {
        fprintf(stderr, "Error on line %zu: operands must be separated by a ','\n", lineno);
        return 1;
    }

    if (mem_ops > 1)
    {
        fprintf(stderr, "Error on line %zu: expected exactly one memory operand\n", lineno);
        return 1;
    }

    if (mem_ops == 1 && tokens[mem_op_start + 1].type == T_C_BRACK)
    {
        fprintf(stderr, "Error on line %zu: empty memory operand\n", lineno);
        return 1;
    }

    if (imm_ops > 1)
    {
        fprintf(stderr, "Error on line %zu: expected exactly one immediate operand\n", lineno);
        return 1;
    }

    if (imm_ops == 1 && tokens[token_count - 1].type != T_NUMBER)
    {
        fprintf(stderr, "Error on line %zu: immediate must be the second operand\n", lineno);
        return 1;
    }

    if (regs_in > 2)
    {
        fprintf(stderr, "Error on line %zu: too many registers in the memory operand\n", lineno);
        return 1;
    }

    if (regs_in >= 1)
    {
        if (regs_in == 2)
        {
            if (tokens[mem_op_start + 1].type != T_REG || tokens[mem_op_start + 2].type != T_PLUS || tokens[mem_op_start + 3].type != T_REG)
            {
                fprintf(stderr, "Error on line %zu: expected '[reg+reg...]' pattern in memory operand\n", lineno);
                return 1;
            }

            TokenType after = tokens[mem_op_start + 4].type;
            if (after != T_PLUS && after != T_MINUS && after != T_C_BRACK)
            {
                fprintf(stderr, "Error on line %zu: invalid token after '[reg+reg' in memory operand\n", lineno);
                return 1;
            }
        }
        else if (regs_in == 1)
        {
            if (tokens[mem_op_start + 1].type != T_REG)
            {
                fprintf(stderr, "Error on line %zu: expected register immediately after '[' in memory operand\n", lineno);
                return 1;
            }

            TokenType after = tokens[mem_op_start + 2].type;
            if (after != T_PLUS && after != T_MINUS && after != T_C_BRACK)
            {
                fprintf(stderr, "Error on line %zu: invalid token after '[reg' in memory operand\n", lineno);
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