#include <stdio.h>  // for fprintf, stderr
#include <stdlib.h> // for free, exit, strtol
#include <string.h> // for strcmp
#include <errno.h>  // for errno

#include "parser.h"

static inline int validate_syntax(const Token *tokens, size_t token_count, size_t lineno, uint8_t *ops_out, size_t *comma_i_out);
static inline int parse_operand(const OperandTokenSpan *tspan, Operand *op_out, size_t lineno);
static inline MnemonicType classify_mnemonic(const char *mnemonic);
static inline void free_tokens(Token *tokens, size_t token_count);

static const struct
{
    const char *name;
    Size size;
    uint8_t reg_code;
} registers[] = {
    {"al", SZ_BYTE, 0x00},
    {"ah", SZ_BYTE, 0x04},
    {"ax", SZ_WORD, 0x00},
    {"cl", SZ_BYTE, 0x01},
    {"ch", SZ_BYTE, 0x05},
    {"cx", SZ_WORD, 0x01},
    {"dl", SZ_BYTE, 0x02},
    {"dh", SZ_BYTE, 0x06},
    {"dx", SZ_WORD, 0x02},
    {"bl", SZ_BYTE, 0x03},
    {"bh", SZ_BYTE, 0x07},
    {"bx", SZ_WORD, 0x03},
    {"sp", SZ_WORD, 0x04},
    {"bp", SZ_WORD, 0x05},
    {"si", SZ_WORD, 0x06},
    {"di", SZ_WORD, 0x07},
    {NULL, SZ_NONE, 0}};

static const struct
{
    const char *base_reg;
    const char *index_reg;
    uint8_t rm_code;
} address_table[] = {
    {"bx", "si", 0x00},
    {"bx", "di", 0x01},
    {"bp", "si", 0x02},
    {"bp", "di", 0x03},
    {"si", NULL, 0x04},
    {"di", NULL, 0x05},
    {"bp", NULL, 0x06}, // 16 bit displacement when MOD is 00
    {"bx", NULL, 0x07},
    {NULL, NULL, 0}};

int parse_tokens(Token *tokens, size_t token_count, size_t lineno, Instruction *inst_out)
{
    size_t comma_i = 0;
    uint8_t operands = 0;
    int result = validate_syntax(tokens, token_count, lineno, &operands, &comma_i);
    if (result != 0)
    {
        free_tokens(tokens, token_count);
        return 1;
    }

    switch (classify_mnemonic(tokens[0].lexeme))
    {
    case T_MOV:
        if (operands != 2)
        {
            fprintf(stderr, "Error on line %zu: 'mov' instruction requires exactly two operands\n", lineno);
            free_tokens(tokens, token_count);
            return 1;
        }
        OperandTokenSpan op1tokens = {.tokens = &tokens[1], .count = comma_i - 1};
        OperandTokenSpan op2tokens = {.tokens = &tokens[comma_i + 1], .count = token_count - (comma_i + 1)};
        Operand op1 = {0}, op2 = {0};
        result = parse_operand(&op1tokens, &op1, lineno);
        if (result != 0)
        {
            free_tokens(tokens, token_count);
            return 1;
        }
        result = parse_operand(&op2tokens, &op2, lineno);
        if (result != 0)
        {
            free_tokens(tokens, token_count);
            return 1;
        }

        // if imm-to-reg and imm size is not explicitly set, then infer it from reg
        if (op1.opType == OP_REG && op2.opType == OP_IMM && !op2.has_explicit_size)
            op2.size = op1.size;

        // if one operand is memory and has no size, infer it from the other
        if (op1.opType == OP_MEM && op1.size == SZ_NONE)
            op1.size = op2.size;
        else if (op2.opType == OP_MEM && op2.size == SZ_NONE)
            op2.size = op1.size;

        if (op1.size == SZ_NONE && op2.size == SZ_NONE)
        {
            fprintf(stderr, "Error on line %zu: operation size not specified\n", lineno);
            free_tokens(tokens, token_count);
            return 1;
        }

        if (op1.size != op2.size)
        {
            fprintf(stderr, "Error on line %zu: operand sizes do not match\n", lineno);
            free_tokens(tokens, token_count);
            return 1;
        }

        inst_out->mnem = T_MOV;
        inst_out->op1 = op1;
        inst_out->op2 = op2;

        break;
    }

    free_tokens(tokens, token_count);
    return 0;
}

// parser_test.c for details
static inline int validate_syntax(const Token *tokens, size_t token_count, size_t lineno, uint8_t *ops_out, size_t *comma_i_out)
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
            *comma_i_out = i;
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
            if (next != T_NUMBER && next != T_REG && next != T_PLUS && next != T_MINUS)
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

    *ops_out = operand_count;
    return 0;
}

static inline int parse_operand(const OperandTokenSpan *tspan, Operand *op_out, size_t lineno)
{
    // Check for size specifier 'byte' or 'word'
    size_t op_start = 0;
    if (tspan->count > 1 && tspan->tokens[0].type == T_SIZE)
    {
        op_out->has_explicit_size = true;
        if (strcmp(tspan->tokens[0].lexeme, "byte") == 0)
            op_out->explicit_size = SZ_BYTE;
        else
            op_out->explicit_size = SZ_WORD;
        op_start = 1;
    }

    const Token *tok0 = &tspan->tokens[op_start];
    // immediate might have a sign as a previous token so taking that into account with tok1
    const Token *tok1 = op_start + 1 < tspan->count ? &tspan->tokens[op_start + 1] : NULL;

    // check for immediate (T_NUMBER or [+/-] T_NUMBER)
    if (tok0->type == T_NUMBER || ((tok0->type == T_PLUS || tok0->type == T_MINUS) && tok1 && tok1->type == T_NUMBER))
    {
        op_out->opType = OP_IMM;

        const char *num_str = NULL;
        int sign = 1;

        if (tok0->type == T_NUMBER)
            num_str = tok0->lexeme;
        else
        {
            sign = (tok0->type == T_MINUS) ? -1 : 1;
            num_str = tok1->lexeme;
        }

        errno = 0;
        char *end = NULL;
        long val = strtol(num_str, &end, 10);
        val *= sign;

        if (errno != 0 || *end != '\0')
        {
            fprintf(stderr, "Error on line %zu: immediate value exceeds valid range\n", lineno);
            return 1;
        }

        if (val < -65536 || val > 65535)
        {
            fprintf(stderr, "Error on line %zu: immediate value exceeds valid range (-65536 to 65535)\n", lineno);
            return 1;
        }

        if (op_out->has_explicit_size)
        {
            if (op_out->explicit_size == SZ_BYTE)
                if (val < -256 || val > 255)
                {
                    fprintf(stderr, "Error on line %zu: immediate value does not fit in a byte (-256 to 255)\n", lineno);
                    return 1;
                }
            op_out->size = op_out->explicit_size;
        }
        op_out->imm.value = (uint16_t)val;
    }
    // check for register (T_REG)
    else if (tok0->type == T_REG)
    {
        op_out->opType = OP_REG;

        for (int i = 0; registers[i].name != NULL; i++)
        {
            if (strcmp(tok0->lexeme, registers[i].name) == 0)
            {
                op_out->size = registers[i].size;
                op_out->reg.reg_code = registers[i].reg_code;
            }
        }
    }
    // check for memory operand
    else if (tok0->type == T_O_BRACK)
    {
        op_out->opType = OP_MEM;

        const char *base_reg = NULL;
        const char *index_reg = NULL;

        int sign = 1;
        int32_t disp_total = 0;

        size_t i = op_start + 1;
        size_t end = tspan->count - 1;
        while (i < end)
        {
            switch (tspan->tokens[i].type)
            {
            case T_REG:
                if (base_reg == NULL)
                {
                    const char *reg_lexeme = tspan->tokens[i].lexeme;

                    for (int j = 0; address_table[j].base_reg != NULL; j++)
                    {
                        if (strcmp(reg_lexeme, address_table[j].base_reg) == 0)
                        {
                            base_reg = reg_lexeme;
                            break;
                        }
                    }

                    if (base_reg == NULL)
                    {
                        fprintf(stderr, "Error on line %zu: invalid base register '%s' in the memory operand\n", lineno, reg_lexeme);
                        return 1;
                    }

                    i++;
                    continue;
                }
                else if (index_reg == NULL)
                {
                    const char *reg_lexeme = tspan->tokens[i].lexeme;

                    if (!(strcmp(base_reg, "bx") == 0 || strcmp(base_reg, "bp") == 0))
                    {
                        fprintf(stderr, "Error on line %zu: base register '%s' cannot be combined with an index register\n", lineno, base_reg);
                        return 1;
                    }

                    if (!(strcmp(reg_lexeme, "si") == 0 || strcmp(reg_lexeme, "di") == 0))
                    {
                        fprintf(stderr, "Error on line %zu: invalid index register '%s' in the memory operand\n", lineno, reg_lexeme);
                        return 1;
                    }

                    index_reg = reg_lexeme;
                    i++;
                    continue;
                }
                break;
            case T_PLUS:
                sign = 1;
                i++;
                continue;
            case T_MINUS:
                sign = -1;
                i++;
                continue;
            case T_NUMBER:
                errno = 0;
                char *end = NULL;
                long val = strtol(tspan->tokens[i].lexeme, &end, 10);

                if (errno != 0 || *end != '\0')
                {
                    fprintf(stderr, "Error on line %zu: number inside the memory operand exceeds valid range\n", lineno);
                    return 1;
                }

                if (base_reg == NULL)
                {
                    if (val < -65536 || val > 65535)
                    {
                        fprintf(stderr, "Error on line %zu: number inside the memory operand exceeds valid range (-65536 to 65535)\n", lineno);
                        return 1;
                    }

                    disp_total += sign * val;

                    if (disp_total < -65536 || disp_total > 65535)
                    {
                        fprintf(stderr, "Error on line %zu: numbers inside the memory operand exceed valid range (-65536 to 65535)\n", lineno);
                        return 1;
                    }
                }
                else
                {
                    if (val < -32768 || val > 32767)
                    {
                        fprintf(stderr, "Error on line %zu: number inside the memory operand exceeds valid range (-32768 to 32767)\n", lineno);
                        return 1;
                    }

                    disp_total += sign * val;

                    if (disp_total < -32768 || disp_total > 32767)
                    {
                        fprintf(stderr, "Error on line %zu: numbers inside the memory operand exceed valid range (-32768 to 32767)\n", lineno);
                        return 1;
                    }
                }
                break;
            default:
                break;
            }
            i++;
        }

        op_out->mem.base_reg = base_reg;
        op_out->mem.index_reg = index_reg;
        op_out->mem.disp_value = (int16_t)disp_total;

        if (base_reg == NULL)
        {
            // special case: direct address [1234]
            // must be MOD=00, R/M=110, and always 2-byte disp
            op_out->mem.disp_size = SZ_WORD;
            op_out->mem.rm_code = 0x06;
        }
        else
        {
            // set R/M
            for (int i = 0; address_table[i].base_reg != NULL; i++)
            {
                if (strcmp(address_table[i].base_reg, base_reg) == 0)
                {
                    if ((address_table[i].index_reg == NULL && index_reg == NULL) ||
                        (address_table[i].index_reg != NULL && index_reg != NULL && strcmp(address_table[i].index_reg, index_reg) == 0))
                    {
                        op_out->mem.rm_code = address_table[i].rm_code;
                        break;
                    }
                }
            }

            // determine disp size
            if (disp_total == 0 && strcmp(base_reg, "bp") != 0)
            {
                // no displacement, and not [bp] - can use MOD = 00
                op_out->mem.disp_size = SZ_NONE;
            }
            else if (disp_total >= -128 && disp_total <= 127)
            {
                op_out->mem.disp_size = SZ_BYTE;
            }
            else
            {
                op_out->mem.disp_size = SZ_WORD;
            }
        }
    }

    if (op_out->has_explicit_size && op_out->explicit_size != op_out->size)
    {
        fprintf(stderr, "Error on line %zu: operand size (%s) does not match specified size (%s)\n", lineno,
                op_out->size == SZ_BYTE ? "byte" : "word",
                op_out->explicit_size == SZ_BYTE ? "byte" : "word");
        return 1;
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

static inline void free_tokens(Token *tokens, size_t token_count)
{
    for (size_t i = 0; i < token_count; i++)
        free(tokens[i].lexeme);
    free(tokens);
}