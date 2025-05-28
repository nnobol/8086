#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <stddef.h>  // for size_t

#define LINE_N_MAX 65535
#define LINE_LEN_MAX 65535
#define LINE_ONE_BITS_DECLARATION "bits 16\n"

typedef enum
{
    T_INVALID,
    T_EOF,
    T_BAD,
    T_MNEMONIC, // mov (and later add, jmp…)
    T_SIZE,     // byte, word
    T_REG,      // al, ax, bx…
    T_NUMBER,   // decimal literal (no binary or hex yet)
    T_PLUS,     // '+'
    T_MINUS,    // '-'
    T_O_BRACK,  // '['
    T_C_BRACK,  // ']'
    T_COMMA,    // ','
    T_COMMENT   // ';'
} TokenType;

typedef struct
{
    TokenType type;
    char *lexeme;  // null-terminated text, caller must free(), allocated in next_token(), not allocated for T_EOF
    size_t line_n; // source line number where this token appeared
} Token;

typedef struct
{
    const char *line_src; // the original line buffer (e.g. "mov ax, [bx + 10]\n")
    size_t pos;           // current index (0 initially)
    size_t line_len;      // length of the line (via strlen)
    size_t line_n;        // the line number (for errors)
} Tokenizer;

int tokenize_line(const char *line_src, size_t line_n, Token **tokens_out, size_t *token_count_out);

#endif