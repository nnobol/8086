#include <stdio.h>   // for fprintf, stderr
#include <ctype.h>   // for isspace, isdigit, isalpha, tolower
#include <string.h>  // for memcpy, strlen, strcmp
#include <stdlib.h>  // for malloc, realloc, free
#include <stdbool.h> // for bool

#include "tokenizer.h"

static int next_token(Tokenizer *tk, Token *t);
static inline char peek(Tokenizer *tk);
static inline void advance(Tokenizer *tk);
static inline bool is_at_end(Tokenizer *tk);
static inline TokenType classify_identifier(const char *s);

static const struct
{
    const char *lexeme;
    TokenType type;
} keyword_map[] = {
    // mnemonics
    {"mov", T_MNEMONIC},
    {"add", T_MNEMONIC},
    {"sub", T_MNEMONIC},
    {"cmp", T_MNEMONIC},
    // size prefixes
    {"byte", T_SIZE},
    {"word", T_SIZE},
    // registers
    {"al", T_REG},
    {"ah", T_REG},
    {"ax", T_REG},
    {"cl", T_REG},
    {"ch", T_REG},
    {"cx", T_REG},
    {"dl", T_REG},
    {"dh", T_REG},
    {"dx", T_REG},
    {"bl", T_REG},
    {"bh", T_REG},
    {"bx", T_REG},
    {"sp", T_REG},
    {"bp", T_REG},
    {"si", T_REG},
    {"di", T_REG},
    // end marker
    {NULL, T_BAD}};

int tokenize_line(const char *line_src, size_t line_n, Token **tokens_out, size_t *token_count_out)
{
    Tokenizer tk = {
        .line_src = line_src,
        .pos = 0,
        .line_len = strlen(line_src),
        .line_n = line_n};

    Token *arr = NULL;
    size_t capacity = 0, t_count = 0;

    while (1)
    {
        Token t;
        int result = next_token(&tk, &t);

        if (result == 1)
        {
            fprintf(stderr, "Error: memory allocation failed during tokenization (next_token)\n");
            for (size_t i = 0; i < t_count; i++)
                free(arr[i].lexeme);
            free(arr);
            return 1;
        }

        if (t.type == T_COMMENT || t.type == T_EOF)
        {
            free(t.lexeme);
            break;
        }

        if (t_count == capacity)
        {
            size_t newcap = capacity ? capacity * 2 : 4;
            Token *tmp = realloc(arr, newcap * sizeof *arr);
            if (!tmp)
            {
                fprintf(stderr, "Error: memory allocation failed while resizing token array (tokenize_line)\n");
                free(t.lexeme);
                for (size_t i = 0; i < t_count; i++)
                    free(arr[i].lexeme);
                free(arr);
                return 1;
            }
            arr = tmp;
            capacity = newcap;
        }

        arr[t_count++] = t;
    }

    if (t_count < capacity)
    {
        Token *shrunk = realloc(arr, t_count * sizeof *arr);
        if (shrunk)
            arr = shrunk;
        // if realloc fails, arr is still valid with the larger cap
    }

    *tokens_out = arr;
    *token_count_out = t_count;
    return 0;
}

static int next_token(Tokenizer *tk, Token *t_out)
{
    t_out->line_n = tk->line_n;
    t_out->lexeme = NULL;
    t_out->type = T_BAD;

    while (!is_at_end(tk))
    {
        char c = peek(tk);
        if (isspace(c))
        {
            advance(tk);
            continue;
        }

        if (c == '+' || c == '-' || c == '[' || c == ']' || c == ',' || c == ';')
        {
            // 1) classify
            if (c == '+')
                t_out->type = T_PLUS;
            else if (c == '-')
                t_out->type = T_MINUS;
            else if (c == '[')
                t_out->type = T_O_BRACK;
            else if (c == ']')
                t_out->type = T_C_BRACK;
            else if (c == ',')
                t_out->type = T_COMMA;
            else
                t_out->type = T_COMMENT;

            // 2) consume and advance in the line
            advance(tk);

            // 3) allocate lexeme
            t_out->lexeme = malloc(2);
            if (!t_out->lexeme)
                return 1;
            t_out->lexeme[0] = c;
            t_out->lexeme[1] = '\0';

            return 0;
        }

        if (isdigit(c))
        {
            // 1) consume an entire run of digits
            size_t start = tk->pos;
            do
            {
                advance(tk);
            } while (isdigit(peek(tk)));

            // 2) build and allocate the lexeme
            size_t len = tk->pos - start;
            t_out->lexeme = malloc(len + 1);
            if (!t_out->lexeme)
                return 1;
            memcpy(t_out->lexeme, tk->line_src + start, len);
            t_out->lexeme[len] = '\0';

            // 3) classify
            t_out->type = T_NUMBER;
            return 0;
        }
        else if (isalpha(c))
        {
            // 1) consume an entire run of letters
            size_t start = tk->pos;
            do
            {
                advance(tk);
            } while (isalpha(peek(tk)));

            // 2) build and allocate the lexeme into a buffer
            size_t len = tk->pos - start;
            char *buf = malloc(len + 1);
            if (!buf)
                return 1;
            memcpy(buf, tk->line_src + start, len);
            buf[len] = '\0';

            // 3) transform each char to lowercase, then classify
            for (size_t i = 0; i < len; i++)
            {
                buf[i] = (char)tolower((unsigned char)buf[i]);
            }
            t_out->lexeme = buf;
            t_out->type = classify_identifier(buf);

            return 0;
        }

        advance(tk);
        t_out->lexeme = malloc(2);
        if (!t_out->lexeme)
            return 1;
        t_out->lexeme[0] = c;
        t_out->lexeme[1] = '\0';
        t_out->type = T_BAD;
        return 0;
    }

    t_out->type = T_EOF;
    return 0;
}

// returns the current character, or '\0' if at end
static inline char peek(Tokenizer *tk)
{
    return is_at_end(tk) ? '\0' : tk->line_src[tk->pos];
}

// advances one character
static inline void advance(Tokenizer *tk)
{
    if (tk->pos < tk->line_len)
        tk->pos++;
}

static inline bool is_at_end(Tokenizer *tk)
{
    return tk->pos >= tk->line_len;
}

// return T_BAD if no match, otherwise the correct TokenType
static inline TokenType classify_identifier(const char *s)
{
    for (int i = 0; keyword_map[i].lexeme != NULL; i++)
    {
        if (strcmp(s, keyword_map[i].lexeme) == 0)
            return keyword_map[i].type;
    }
    return T_BAD;
}