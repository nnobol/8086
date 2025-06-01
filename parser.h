#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>
#include <stdbool.h>

#include "tokenizer.h" // for Token, TokenType

typedef enum
{
    T_MOV,
} MnemonicType;

typedef enum
{
    OP_NONE,
    OP_REG, // register-to-register or register-to-memory
    OP_IMM, // immediate (e.g. mov ax, 123)
    OP_MEM  // memory operand […]
} OperandType;

typedef enum
{
    SZ_NONE,
    SZ_BYTE, // 8-bit
    SZ_WORD  // 16-bit
} OperandSize;

typedef struct
{
    OperandType opType;
    OperandSize size;
    bool has_explicit_size;    // true for OP_REG and OP_IMM and if 'byte' or 'word' written before operand
    OperandSize explicit_size; // 'byte' (8-bit) or 'word' (16-bit)

    union
    {
        struct
        {
            uint16_t value;
        } imm;
        struct
        {
            uint8_t reg_code;
        } reg;
        struct
        {
            const char *base_reg;
            const char *index_reg;
            int16_t disp;
        } mem;
    };
} Operand;

typedef struct
{
    MnemonicType mnem;
    Operand op1;
    Operand op2;
} Instruction;

typedef struct
{
    const Token *tokens;
    size_t count;
} OperandTokenSpan;

int parse_tokens(Token *tokens, size_t token_count, size_t lineno, Instruction *inst_out);

#endif