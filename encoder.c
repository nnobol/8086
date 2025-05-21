#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include "./types.h"

Register registers[] = {
    {"al", 8, 0x00},
    {"cl", 8, 0x01},
    {"dl", 8, 0x02},
    {"bl", 8, 0x03},
    {"ah", 8, 0x04},
    {"ch", 8, 0x05},
    {"dh", 8, 0x06},
    {"bh", 8, 0x07},
    {"ax", 16, 0x00},
    {"cx", 16, 0x01},
    {"dx", 16, 0x02},
    {"bx", 16, 0x03},
    {"sp", 16, 0x04},
    {"bp", 16, 0x05},
    {"si", 16, 0x06},
    {"di", 16, 0x07},
    {"", 0, 0}};

AddressEncoding effectiveAddressTable[] = {
    {"bx", "si", 0x00},
    {"bx", "di", 0x01},
    {"bp", "si", 0x02},
    {"bp", "di", 0x03},
    {"si", "", 0x04},
    {"di", "", 0x05},
    {"bp", "", 0x06}, // 16 bit displacement when MOD is 00
    {"bx", "", 0x07},
    {"", "", 0}};

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Error: invalid number of arguments, expected 2\n");
        fprintf(stderr, "Usage: my-program input.asm output.bin\n");
        return 1;
    }

    const char *input_filename = argv[1];
    const char *output_filename = argv[2];

    size_t len = strlen(input_filename);
    if (len >= 4 && strcmp(input_filename + len - 4, ".asm") != 0)
    {
        fprintf(stderr, "Error: input file does not end with .asm\n");
        return 1;
    }

    FILE *input = fopen(input_filename, "r");
    if (!input)
    {
        perror("fopen input file");
        return 1;
    }

    FILE *output = fopen(output_filename, "wb");
    if (!output)
    {
        perror("fopen output file");
        fclose(input);
        return 1;
    }

    char line[MAX_LINE_SIZE];
    int lineno = 0;
    while (fgets(line, sizeof(line), input))
    {
        lineno++;
        lineToLower(line);

        if (lineno == 1)
        {
            if (strlen(line) != 8 || strcmp(line, "bits 16\n") != 0)
            {
                fprintf(stderr, "Error: expected just 'bits 16' on line 1\n");
                fclose(input);
                fclose(output);
                return 1;
            }
            continue;
        }

        uint8_t token_count;
        char *tokens[MAX_LINE_TOKENS];
        if (tokenizeLine(line, tokens, &token_count, &lineno) != 0)
        {
            freeTokens(tokens, token_count);
            fclose(input);
            fclose(output);
            return 1;
        }
        if (token_count == 0)
        {
            continue;
        }

        MnemonicType mType = getMnemonicType(tokens[0]);
        switch (mType)
        {
        case MNEMONIC_MOV:
            if (token_count != 3)
            {
                fprintf(stderr, "Error: mov expects 2 operands\n");
                freeTokens(tokens, token_count);
                fclose(input);
                fclose(output);
                return 1;
            }

            Operand op1 = {0}, op2 = {0};
            if (analyzeOperand(tokens[1], &op1, &lineno) != 0 || analyzeOperand(tokens[2], &op2, &lineno) != 0)
            {
                freeTokens(tokens, token_count);
                fclose(input);
                fclose(output);
                return 1;
            }

            if (validateOpCombination(&op1, &op2) != 0)
            {
                fprintf(stderr, "Error: invalid operand combination for 'mov'\n");
                fclose(input);
                fclose(output);
                return 1;
            }

            // 1) register-to-register MOV
            if (op1.type == OP_REG && op2.type == OP_REG)
            {
                if (op1.reg->size != op2.reg->size)
                {
                    fprintf(stderr, "Error: invalid reg to reg operation for 'mov', register sizes must be the same\n");
                    fclose(input);
                    fclose(output);
                    return 1;
                }
                uint8_t bytes[2]; // 2 byte instruction
                bytes[0] = 0x88;  // MOV opcode on byte 1
                if (op1.reg->size == 16)
                {
                    bytes[0] |= 0x01; // Set W=1 on byte 1
                }
                bytes[1] = 0xC0;                  // MOD on byte 2
                bytes[1] |= (op2.reg->mask << 3); // REG (source reg, bits 5–3) on byte 2
                bytes[1] |= op1.reg->mask;        // R/M (dest reg, bits 2–0) on byte 2
                fwrite(bytes, 1, 2, output);
            }
            // 2) immediate-to-register MOV
            else if (op1.type == OP_REG && op2.type == OP_IMM)
            {
                if (op2.imm->size > op1.reg->size)
                {
                    fprintf(stderr, "Error: immediate value too large for 8-bit register\n");
                    free(op2.imm);
                    fclose(input);
                    fclose(output);
                    return 1;
                }
                uint8_t bytes[3]; // at most 3 instruction bytes
                size_t count = 2;

                bytes[0] = 0xB0 | op1.reg->mask; // MOV opcode + register on byte 1
                bytes[1] = op2.imm->val;         // Low byte on byte 2
                if (op1.reg->size == 16)
                {
                    bytes[0] |= 0x08;                   // Set W=1 on byte 1
                    bytes[count++] = op2.imm->val >> 8; // High byte on byte 3
                }
                fwrite(bytes, 1, count, output);
            }
            break;

        case MNEMONIC_INVALID:
            fprintf(stderr, "Unknown instruction or unimplemented mnemonic on line %d: '%s'\n", lineno, tokens[0]);
            freeTokens(tokens, token_count);
            fclose(input);
            fclose(output);
            return 1;

        default:
            freeTokens(tokens, token_count);
            fclose(input);
            fclose(output);
            return 1;
        }
    }

    fclose(input);
    fclose(output);
    return 0;
}

void lineToLower(char *line)
{
    for (int i = 0; line[i]; i++)
    {
        line[i] = tolower(line[i]);
    }
}

int tokenizeLine(char *line, char *tokens[], int *token_count, int *lineno)
{
    *token_count = 0;

    while (*line != '\0' && *token_count < MAX_LINE_TOKENS)
    {
        while (*line == ',' || *line == ' ' || *line == '\t' || *line == '\n')
        {
            line++;
        }

        if (*line == '\0')
        {
            break;
        }

        char *start = line;
        int len = 0;

        if (*line == '[')
        {
            line++;
            while (*line != '\0' && *line != ']')
            {
                line++;
            }
            if (*line != ']')
            {
                fprintf(stderr, "Error: missing closing ']' on line %d\n", *lineno);
                return 1;
            }
            line++;
            len = line - start;
        }
        else
        {
            while (*line != '\0' && *line != ' ' && *line != '\t' && *line != ',' && *line != '\n')
            {
                line++;
            }
            len = line - start;
        }

        char *token = malloc(len + 1);
        if (!token)
        {
            perror("malloc error in tokenizeLine");
            return 1;
        }

        strncpy(token, start, len);
        token[len] = '\0';

        tokens[*token_count] = token;
        (*token_count)++;
    }

    return 0;
}

MnemonicType getMnemonicType(const char *mnemonic)
{
    if (strcmp(mnemonic, "mov") == 0)
        return MNEMONIC_MOV;
    return MNEMONIC_INVALID;
}

int analyzeOperand(const char *op, Operand *out, int *lineno)
{
    // check if the operand is a register
    for (int i = 0; registers[i].name[0] != '\0'; i++)
    {
        if (strcmp(op, registers[i].name) == 0)
        {
            out->type = OP_REG;
            out->reg = &registers[i];
            return 0;
        }
    }

    // check if operand is direct memory access or effective address calculation
    if (op[0] == '[')
    {
        Memory memOp = {0};
        if (analyzeMemOperand(op, &memOp, lineno) != 0)
        {
            return 1;
        }
        out->type = OP_MEM;
        return 0;
    }

    // Try to parse signed decimal immediate
    errno = 0;
    char *end;
    long val = strtol(op, &end, 10);
    if (*end != '\0' || errno != 0)
    {
        fprintf(stderr, "Error: invalid immediate value on line %d: '%s'\n", *lineno, op);
        return 1;
    }

    if (val < -65536 || val > 65535)
    {
        fprintf(stderr, "Error: immediate value out of range on line %d: '%s' (allowed: -65536 to 65535)\n", *lineno, op);
        return 1;
    }

    Immediate *imm = malloc(sizeof(Immediate));
    if (!imm)
    {
        perror("malloc error in analyzeOperand for immediate");
        return 1;
    }

    imm->val = (uint16_t)val;
    imm->size = (val >= -256 && val <= 255) ? 8 : 16;

    out->type = OP_IMM;
    out->imm = imm;

    return 0;
}

int analyzeMemOperand(const char *token, Memory *out, int *lineno)
{
    size_t len = strlen(token);

    if (token[0] != '[' || token[len - 1] != ']')
    {
        fprintf(stderr, "Error: malformed memory operand on line %d: '%s'\n", *lineno, token);
        return 1;
    }

    char inner[MAX_LINE_SIZE];
    strncpy(inner, token + 1, len - 2);
    inner[len - 2] = '\0';

    char *expr = inner;

    while (*expr != '\0')
    {
        while (*expr != ' ' || *expr != '\t')
        {
            expr++;
        }

        if (*expr == '\0')
        {
            fprintf(stderr, "Error: empty memory operand on line %d: '%s'\n", *lineno, token);
            return 1;
        }

        int val;
        if (isPureNumericExpression(expr, &val, lineno) != 0)
        {
            return 1;
        }

        // maybe have fields on the Memory type to account for pure numeric value
        // check if pure numeric expression first and evaluate it if so
        // valid range val < -65536 || val > 65535
    }
}

int isPureNumericExpression(const char *expr, int *value, int *lineno)
{
    int total = 0;
    int sign = 1;

    while (*expr != '\0')
    {
        if (*expr == '+')
        {
            sign = 1;
            expr++;
        }
        else if (*expr == '-')
        {
            sign = -1;
            expr++;
        }

        if (!isdigit(*expr))
        {
            fprintf(stderr, "Error: invalid memory operand expression on line %d: '%s'\n", *lineno, expr);
            return 1;
        }
    }

    return 0;
}

int validateOpCombination(const Operand *op1, const Operand *op2)
{
    if (op1->type == OP_IMM)
    {
        return 1;
    }

    if (op1->type == OP_MEM && op2->type == OP_MEM)
    {
        return 1;
    }

    return 0;
}

void freeTokens(char *tokens[], int token_count)
{
    for (int i = 0; i < token_count; i++)
    {
        free(tokens[i]);
    }
}