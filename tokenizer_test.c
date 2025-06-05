#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tokenizer.c"

void expect_token(const Token *t, TokenType expected_type, const char *expected_lexeme, size_t expected_line)
{
    assert(t->type == expected_type);
    assert(strcmp(t->lexeme, expected_lexeme) == 0);
    assert(t->line_n == expected_line);
}

void test_mov_simple()
{
    const char *line = "mov ax, bx";
    Token *tokens = NULL;
    size_t token_count = 0;
    int result = tokenize_line(line, 1, &tokens, &token_count);
    assert(result == 0);
    // expected: mov, ax, ',', bx
    assert(token_count == 4);
    expect_token(&tokens[0], T_MNEMONIC, "mov", 1);
    expect_token(&tokens[1], T_REG, "ax", 1);
    expect_token(&tokens[2], T_COMMA, ",", 1);
    expect_token(&tokens[3], T_REG, "bx", 1);

    for (size_t i = 0; i < token_count; i++)
        free(tokens[i].lexeme);
    free(tokens);
}

void test_mov_memory()
{
    const char *line = "mov word, [bp+123] ; comment";
    Token *tokens = NULL;
    size_t token_count = 0;
    int result = tokenize_line(line, 42, &tokens, &token_count);
    assert(result == 0);
    // expected: mov, word, ',', '[', bp, '+', 123, ']'
    // comment should be stripped
    assert(token_count == 8);
    int i = 0;
    expect_token(&tokens[i++], T_MNEMONIC, "mov", 42);
    expect_token(&tokens[i++], T_SIZE, "word", 42);
    expect_token(&tokens[i++], T_COMMA, ",", 42);
    expect_token(&tokens[i++], T_O_BRACK, "[", 42);
    expect_token(&tokens[i++], T_REG, "bp", 42);
    expect_token(&tokens[i++], T_PLUS, "+", 42);
    expect_token(&tokens[i++], T_NUMBER, "123", 42);
    expect_token(&tokens[i++], T_C_BRACK, "]", 42);

    for (size_t j = 0; j < token_count; j++)
        free(tokens[j].lexeme);
    free(tokens);
}

void test_numbers_and_bad()
{
    const char *line = "123 abc 45,gh";
    Token *tokens = NULL;
    size_t token_count = 0;
    int result = tokenize_line(line, 7, &tokens, &token_count);
    assert(result == 0);
    // expected: 123, abc, 45, ',', gh
    assert(token_count == 5);
    expect_token(&tokens[0], T_NUMBER, "123", 7);
    expect_token(&tokens[1], T_BAD, "abc", 7);
    expect_token(&tokens[2], T_NUMBER, "45", 7);
    expect_token(&tokens[3], T_COMMA, ",", 7);
    expect_token(&tokens[4], T_BAD, "gh", 7);

    for (size_t i = 0; i < token_count; i++)
        free(tokens[i].lexeme);
    free(tokens);
}

void test_empty_line()
{
    const char *line = "   \t  ";
    Token *tokens = NULL;
    size_t token_count = 0;
    int result = tokenize_line(line, 100, &tokens, &token_count);
    assert(result == 0);
    assert(token_count == 0);
    if (tokens)
        free(tokens);
}

int main(void)
{
    printf("Running tokenizer tests...\n");
    test_mov_simple();
    test_mov_memory();
    test_numbers_and_bad();
    test_empty_line();
    printf("All tests passed!\n");
    return 0;
}