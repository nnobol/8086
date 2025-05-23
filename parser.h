#ifndef PARSER_H
#define PARSER_H

// #include <stdint.h>

#include "tokenizer.h" // for Token, TokenType

typedef enum
{
    T_MOV,
} InstructionType;

typedef enum
{
    OP_REG, // register-to-register or register-to-memory
    OP_IMM, // immediate (e.g. mov ax, 123)
    OP_MEM  // memory operand […]
} OperandKind;

static inline int validate_syntax(Token *tokens, size_t token_count, size_t lineno);
static inline int check_bad_tokens(const Token *tokens, size_t token_count, size_t lineno);
static inline int check_mnemonic(const Token *tokens, size_t lineno);
static inline int check_brackets(const Token *tokens, size_t token_count, size_t lineno);
static inline int check_commas(const Token *tokens, size_t token_count, size_t lineno);

#endif