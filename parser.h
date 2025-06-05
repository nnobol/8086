#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>  // for uint8_t, uint16_t, int16_t, int32_t, int64_t
#include <stdbool.h> // for bool

#include "tokenizer.h" // for Token, TokenType

typedef enum
{
    SZ_NONE,
    SZ_BYTE, // 8-bit
    SZ_WORD  // 16-bit
} Size;

typedef enum
{
    T_MOV,
} MnemonicType;

typedef enum
{
    OP_NONE,
    OP_REG,
    OP_IMM,
    OP_MEM
} OperandType;

typedef struct
{
    const Token *tokens;
    size_t count;
} OperandTokenSpan;

typedef struct
{
    OperandType opType;
    Size size;
    bool has_explicit_size; // true for OP_REG and OP_IMM and if 'byte' or 'word' written before operand
    Size explicit_size;     // 'byte' (8-bit) or 'word' (16-bit)

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
            uint8_t rm_code;
            Size disp_size;
            int16_t disp_value;
        } mem;
    };
} Operand;

typedef struct
{
    MnemonicType mnem;
    Operand op1;
    Operand op2;
} Instruction;

int parse_tokens(Token *tokens, size_t token_count, size_t lineno, Instruction *inst_out);

#endif