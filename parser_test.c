#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tokenizer.h"
#include "parser.h"

static FILE *stderr_save;
static FILE *stderr_tmp;

static void begin_capture_stderr(void)
{
    stderr_save = stderr;
    stderr_tmp = tmpfile();
    assert(stderr_tmp != NULL);
    stderr = stderr_tmp;
}

static char *end_capture_stderr(void)
{
    fseek(stderr_tmp, 0, SEEK_SET);
    char buf[1024];
    size_t n = fread(buf, 1, sizeof(buf) - 1, stderr_tmp);
    buf[n] = '\0';
    stderr = stderr_save;
    fclose(stderr_tmp);

    char *out = malloc(n + 1);
    assert(out != NULL);
    memcpy(out, buf, n + 1);
    return out;
}

static void expect_parse_error(const char *line, const char *want_msg)
{
    // 1) tokenize
    Token *tokens = NULL;
    size_t lineno = 10;
    size_t n = 0;
    int tr = tokenize_line(line, lineno, &tokens, &n);
    assert(tr == 0);

    // 2) capture stderr
    begin_capture_stderr();
    Instruction dummy;
    int pr = parse_tokens(tokens, n, lineno, &dummy);
    char *out = end_capture_stderr();

    // 3) assert return != 0 and message contains want_msg
    assert(pr != 0);
    assert(strstr(out, want_msg) != NULL);

    // 4) cleanup
    free(out);
}

static void test_bad_token(void)
{
    expect_parse_error("mov ax, bad", "Error: invalid token on line 10: 'bad'");
}

static void test_bad_mnemonic_position(void)
{
    expect_parse_error("ax mov, bx", "Error: invalid first token on line 10: expected a mnemonic, got 'ax'");
}

static void test_bad_brackets(void)
{
    expect_parse_error("mov ax, ]", "Error on line 10: closing ']' without an opening '['");
    expect_parse_error("mov ax, [", "Error on line 10: opening '[' without a matching ']'");
    expect_parse_error("mov ax, [[100]]", "Error on line 10: nested '[' is not allowed");
    expect_parse_error("mov ax, [[100]", "Error on line 10: nested '[' is not allowed");
    expect_parse_error("mov ax, []", "Error on line 10: empty memory operand");
}

static void test_bad_commas(void)
{
    expect_parse_error("mov ax 5,", "Error on line 10: ',' not allowed at that position");
    expect_parse_error("mov, ax 5", "Error on line 10: ',' not allowed at that position");
    expect_parse_error("mov ax,, 5", "Error on line 10: expected exactly one ','");
}

static void test_bad_size_specifiers(void)
{
    expect_parse_error("mov ax, 5 byte", "Error on line 10: size specifier 'byte' not allowed as the last token");
    expect_parse_error("mov ax, 5 word", "Error on line 10: size specifier 'word' not allowed as the last token");
    expect_parse_error("mov byte, ax 5", "Error on line 10: cannot write a size specifier before a ','");
    expect_parse_error("mov word ax byte, 5", "Error on line 10: cannot write a size specifier before a ','");
    expect_parse_error("mov word ax word word 600", "Error on line 10: more than 2 size specifiers found in the line");
    expect_parse_error("mov word ax byte 100", "Error on line 10: size specifiers of different sizes is not allowed");
}

static void test_bad_numbers(void)
{
    expect_parse_error("mov ax, 5 10", "Error on line 10: two numbers not allowed");
    expect_parse_error("mov 5, ax", "Error on line 10: number not allowed at that position");
    expect_parse_error("mov ax, 5 bx", "Error on line 10: number not allowed at that position");
    expect_parse_error("mov ax, [5+10-20 20]", "Error on line 10: the number of sign symbols and numbers don't match");
}

static void test_bad_sign_symbols(void)
{
    expect_parse_error("mov -ax, 10", "Error on line 10: sign symbols not allowed at that position");
    expect_parse_error("mov +ax, 10", "Error on line 10: sign symbols not allowed at that position");
    expect_parse_error("mov ax, +bx", "Error on line 10: sign symbols outside of the memory operand must be followed by a number");
    expect_parse_error("mov ax, -bx", "Error on line 10: sign symbols outside of the memory operand must be followed by a number");
    expect_parse_error("mov ax, [-bx]", "Error on line 10: the minus sign symbol inside of the memory operand must be followed by a number");
    expect_parse_error("mov ax, [+bx]", "Error on line 10: the plus sign symbol inside of the memory operand not allowed for the first regiser");
    expect_parse_error("mov ax, [bx+]", "Error on line 10: the plus sign symbol inside of the memory operand must be followed by a register or a number");
}

static void test_bad_registers(void)
{
    expect_parse_error("mov ax, bx cx", "Error on line 10: too many registers on the line");
    expect_parse_error("mov ax, [bp bx cx]", "Error on line 10: too many registers on the line");
    expect_parse_error("mov ax bx", "Error on line 10: the first register operand must be followed by a ','");
    expect_parse_error("mov ax, bx [cx]", "Error on line 10: too many registers on the line");
    expect_parse_error("mov ax, bx 5", "Error on line 10: the second register operand must the last token");
    expect_parse_error("mov ax, bx [5]", "Error on line 10: the second register operand must the last token");
    expect_parse_error("mov [ax bx cx], 5", "Error on line 10: too many registers on the line");
    expect_parse_error("mov ax, [ax bx]", "Error on line 10: the first register inside the memory operand must be followed by a '+'");
}

int main(void)
{
    printf("Running parser negative tests...\n");
    test_bad_token();
    test_bad_mnemonic_position();
    test_bad_brackets();
    test_bad_commas();
    test_bad_size_specifiers();
    test_bad_numbers();
    test_bad_sign_symbols();
    test_bad_registers();
    printf("All parser tests passed!\n");
    return 0;
}