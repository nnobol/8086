#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h> // for _dup, _dup2, _fileno
#define dup _dup
#define dup2 _dup2
#define fileno _fileno
#endif

#include "tokenizer.c"
#include "parser.c"

static FILE *stderr_tmp;

#ifdef _WIN32
static int stderr_fd_backup;
#else
static FILE *stderr_save;
#endif

static void begin_capture_stderr(void)
{
#ifdef _WIN32
    errno_t err = tmpfile_s(&stderr_tmp);
    assert(err == 0 && stderr_tmp != NULL);
    stderr_fd_backup = dup(fileno(stderr));
    dup2(fileno(stderr_tmp), fileno(stderr));
#else
    stderr_tmp = tmpfile();
    assert(stderr_tmp != NULL);
    stderr_save = stderr;
    stderr = stderr_tmp;
#endif
}

static char *end_capture_stderr(void)
{
    fseek(stderr_tmp, 0, SEEK_SET);
    char buf[1024];
    size_t n = fread(buf, 1, sizeof(buf) - 1, stderr_tmp);
    buf[n] = '\0';

#ifdef _WIN32
    dup2(stderr_fd_backup, fileno(stderr));
    _close(stderr_fd_backup);
#else
    stderr = stderr_save;
#endif

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

static void test_bad_tokens(void)
{
    expect_parse_error("mov ax, bad", "Error on line 10: invalid token 'bad'");
    expect_parse_error("bad", "Error on line 10: invalid token 'bad'");
    expect_parse_error("mov bad bad2 bad3", "Error on line 10: invalid token 'bad'");
}

static void test_invalid_instruction_structure(void)
{
    expect_parse_error("ax mov, bx", "Error on line 10: first token should be a valid mnemonic");
    expect_parse_error("ax, bx", "Error on line 10: first token should be a valid mnemonic");
    expect_parse_error("mov mov", "Error on line 10: expected exactly one mnemonic");
}

static void test_operand_count_and_positioning(void)
{
    expect_parse_error("mov ax bx", "Error on line 10: operands must be separated by a ','");
    expect_parse_error("mov ax 5", "Error on line 10: operands must be separated by a ','");
    expect_parse_error("mov ax [100]", "Error on line 10: operands must be separated by a ','");
    expect_parse_error("mov [100] 5", "Error on line 10: operands must be separated by a ','");
    expect_parse_error("mov [100] ax", "Error on line 10: operands must be separated by a ','");
    expect_parse_error("mov [100] [100]", "Error on line 10: operands must be separated by a ','");
    expect_parse_error("mov 5 5", "Error on line 10: operands must be separated by a ','");
    expect_parse_error("mov ax bx cx", "Error on line 10: too many operands (maximum 2 allowed)");
    expect_parse_error("mov [100] [100] [100]", "Error on line 10: too many operands (maximum 2 allowed)");
    expect_parse_error("mov 5 5 5", "Error on line 10: too many operands (maximum 2 allowed)");
    expect_parse_error("mov ax 5 [100]", "Error on line 10: too many operands (maximum 2 allowed)");
    expect_parse_error("mov [100], [100]", "Error on line 10: expected exactly one memory operand");
    expect_parse_error("mov 5, 5", "Error on line 10: expected exactly one immediate operand");
    expect_parse_error("mov 5, ax", "Error on line 10: immediate must be the second operand");
}

static void test_memory_operand_syntax(void)
{
    expect_parse_error("mov ax, ]", "Error on line 10: closing ']' without an opening '['");
    expect_parse_error("mov ax, [", "Error on line 10: opening '[' without a matching ']'");
    expect_parse_error("mov ax, [[100]]", "Error on line 10: nested '[' is not allowed");
    expect_parse_error("mov ax, [[100]", "Error on line 10: nested '[' is not allowed");
    expect_parse_error("mov ax, []", "Error on line 10: empty memory operand");
    expect_parse_error("mov ax, [bp bx cx]", "Error on line 10: too many registers in the memory operand");
    expect_parse_error("mov ax, [ax bx]", "Error on line 10: expected '[reg+reg...]' pattern in memory operand");
    expect_parse_error("mov ax, [ax+bx 20] ", "Error on line 10: invalid token after '[reg+reg' in memory operand");
    expect_parse_error("mov ax, [20+bx] ", "Error on line 10: expected register immediately after '[' in memory operand");
    expect_parse_error("mov ax, [+bx]", "Error on line 10: expected register immediately after '[' in memory operand");
    expect_parse_error("mov ax, [bx 20] ", "Error on line 10: invalid token after '[reg' in memory operand");
    expect_parse_error("mov ax, [-bx]", "Error on line 10: '-' symbol inside the memory operand must be followed by a number");
    expect_parse_error("mov ax, [bx-cx]", "Error on line 10: '-' symbol inside the memory operand must be followed by a number");
    expect_parse_error("mov ax, [5+10-20-]", "Error on line 10: '-' symbol inside the memory operand must be followed by a number");
    expect_parse_error("mov ax, [bx+]", "Error on line 10: '+' symbol inside the memory operand must be followed by a number or a register");
    expect_parse_error("mov ax, [5+10-20+]", "Error on line 10: '+' symbol inside the memory operand must be followed by a number or a register");
    expect_parse_error("mov ax, [5+10-20 20]", "Error on line 10: number inside memory operand must be followed by '+' or '-' or closing ']'");
    expect_parse_error("mov ax, [5+10-20 bx]", "Error on line 10: number inside memory operand must be followed by '+' or '-' or closing ']'");
    expect_parse_error("mov ax, [5+10-20 byte]", "Error on line 10: number inside memory operand must be followed by '+' or '-' or closing ']'");
    expect_parse_error("mov ax, [5+10-20 word]", "Error on line 10: number inside memory operand must be followed by '+' or '-' or closing ']'");
    expect_parse_error("mov ax, [word bp]", "Error on line 10: size specifier not allowed inside the memory operand");
    expect_parse_error("mov ax, [byte 5]", "Error on line 10: size specifier not allowed inside the memory operand");
}

static void test_bad_commas(void)
{
    expect_parse_error("mov ax,", "Error on line 10: unexpected end of input after ','");
    expect_parse_error("mov ax 5,", "Error on line 10: unexpected end of input after ','");
    expect_parse_error("mov, ax 5", "Error on line 10: ',' must be between two operands");
    expect_parse_error("mov ax,, 5", "Error on line 10: expected exactly one ','");
    expect_parse_error("mov ax, 5, 10", "Error on line 10: expected exactly one ','");
    expect_parse_error("mov [,] 5", "Error on line 10: ',' not allowed inside the memory operand");
    expect_parse_error("mov [ax,] 5", "Error on line 10: ',' not allowed inside the memory operand");
}

static void test_bad_size_specifiers(void)
{
    expect_parse_error("mov ax, 5 byte", "Error on line 10: unexpected end of input after 'byte'");
    expect_parse_error("mov ax, 5 word", "Error on line 10: unexpected end of input after 'word'");
    expect_parse_error("mov byte, ax 5", "Error on line 10: size specifier must be followed by an immediate or a register");
    expect_parse_error("mov word ax byte, 5", "Error on line 10: size specifier must be followed by an immediate or a register");
    expect_parse_error("mov word ax byte [100]", "Error on line 10: size specifier must be followed by an immediate or a register");
    expect_parse_error("mov word ax word word bx", "Error on line 10: size specifier must be followed by an immediate or a register");
}

static void test_bad_sign_symbols(void)
{
    expect_parse_error("mov -ax, 10", "Error on line 10: sign symbols outside the memory operand must be followed by a number");
    expect_parse_error("mov +ax, 10", "Error on line 10: sign symbols outside the memory operand must be followed by a number");
    expect_parse_error("mov ax, +bx", "Error on line 10: sign symbols outside the memory operand must be followed by a number");
    expect_parse_error("mov ax, -bx", "Error on line 10: sign symbols outside the memory operand must be followed by a number");
    expect_parse_error("mov ax, +[100]", "Error on line 10: sign symbols outside the memory operand must be followed by a number");
    expect_parse_error("mov ax, -[100]", "Error on line 10: sign symbols outside the memory operand must be followed by a number");
    expect_parse_error("mov ax, --100", "Error on line 10: sign symbols outside the memory operand must be followed by a number");
    expect_parse_error("mov ax, ++100", "Error on line 10: sign symbols outside the memory operand must be followed by a number");
}

static void test_bad_syntax(void)
{
    test_bad_tokens();
    test_invalid_instruction_structure();
    test_operand_count_and_positioning();
    test_memory_operand_syntax();
    test_bad_commas();
    test_bad_size_specifiers();
    test_bad_sign_symbols();
}

static void test_semantic_mov_errors(void)
{
    expect_parse_error("mov ax", "Error on line 10: 'mov' instruction requires exactly two operands");
    expect_parse_error("mov byte ax, 5", "Error on line 10: operand size (word) does not match specified size (byte)");
    expect_parse_error("mov word al, 5", "Error on line 10: operand size (byte) does not match specified size (word)");
    expect_parse_error("mov al, byte 500", "Error on line 10: immediate value does not fit in a byte (-256 to 255)");
    expect_parse_error("mov al, word 5", "Error on line 10: operand sizes do not match");
    expect_parse_error("mov ax, 999999999999999999999999", "Error on line 10: immediate value exceeds valid range");
    expect_parse_error("mov ax, -65537", "Error on line 10: immediate value exceeds valid range (-65536 to 65535)");
    expect_parse_error("mov ax, 65536", "Error on line 10: immediate value exceeds valid range (-65536 to 65535)");
    expect_parse_error("mov ax, [ax]", "Error on line 10: invalid base register 'ax' in the memory operand");
    expect_parse_error("mov ax, [si+di]", "Error on line 10: base register 'si' cannot be combined with an index register");
    expect_parse_error("mov ax, [di+si]", "Error on line 10: base register 'di' cannot be combined with an index register");
    expect_parse_error("mov ax, [bp+ax]", "Error on line 10: invalid index register 'ax' in the memory operand");
    expect_parse_error("mov ax, [bx+cx]", "Error on line 10: invalid index register 'cx' in the memory operand");
    expect_parse_error("mov ax, [999999999999999999999999]", "Error on line 10: number inside the memory operand exceeds valid range");
    expect_parse_error("mov ax, [-999999999999999999999999]", "Error on line 10: number inside the memory operand exceeds valid range");
    expect_parse_error("mov ax, [+999999999999999999999999]", "Error on line 10: number inside the memory operand exceeds valid range");
    expect_parse_error("mov ax, [bp+999999999999999999999999]", "Error on line 10: number inside the memory operand exceeds valid range");
    expect_parse_error("mov ax, [-65537]", "Error on line 10: number inside the memory operand exceeds valid range (-65536 to 65535)");
    expect_parse_error("mov ax, [65536]", "Error on line 10: number inside the memory operand exceeds valid range (-65536 to 65535)");
    expect_parse_error("mov ax, [40000+40000]", "Error on line 10: numbers inside the memory operand exceed valid range (-65536 to 65535)");
    expect_parse_error("mov ax, [-40000-40000]", "Error on line 10: numbers inside the memory operand exceed valid range (-65536 to 65535)");
    expect_parse_error("mov ax, [bp-32769]", "Error on line 10: number inside the memory operand exceeds valid range (-32768 to 32767)");
    expect_parse_error("mov ax, [bp+32768]", "Error on line 10: number inside the memory operand exceeds valid range (-32768 to 32767)");
    expect_parse_error("mov ax, [bp+17000+17000]", "Error on line 10: numbers inside the memory operand exceed valid range (-32768 to 32767)");
    expect_parse_error("mov ax, [bp-17000-17000]", "Error on line 10: numbers inside the memory operand exceed valid range (-32768 to 32767)");
    expect_parse_error("mov ax, al", "Error on line 10: operand sizes do not match");
    expect_parse_error("mov word ax, byte al", "Error on line 10: operand sizes do not match");
    expect_parse_error("mov [100], 5", "Error on line 10: operation size not specified");
}

int main(void)
{
    printf("Running parser negative tests...\n");
    test_bad_syntax();
    test_semantic_mov_errors();
    printf("All parser tests passed!\n");
    return 0;
}