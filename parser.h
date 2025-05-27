#ifndef PARSER_H
#define PARSER_H

#include "tokenizer.h" // for Token, TokenType

typedef enum
{
    T_MOV,
} MnemonicType;

typedef enum
{
    OP_REG, // register-to-register or register-to-memory
    OP_IMM, // immediate (e.g. mov ax, 123)
    OP_MEM  // memory operand […]
} OperandType;

typedef struct
{
    MnemonicType mnem;
} Instruction;

int parse_tokens(Token *tokens, size_t token_count, size_t lineno, Instruction *inst_out);

#endif