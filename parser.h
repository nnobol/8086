#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>
#include "tokenizer.h" // for Token, TokenType

typedef enum
{
    OP_REG, // register-to-register or register-to-memory
    OP_IMM, // immediate (e.g. mov ax, 123)
    OP_MEM  // memory operand […]
} OperandKind;

#endif