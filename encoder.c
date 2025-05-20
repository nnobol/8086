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
        fprintf(stderr, "Error: file does not end with .asm\n");
        return 1;
    }

    FILE *input = fopen(input_filename, "r");
    if (!input)
    {
        perror("fopen input");
        return 1;
    }

    FILE *output = fopen(output_filename, "wb");
    if (!output)
    {
        perror("fopen output");
        fclose(input);
        return 1;
    }

    char line[256];
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

        uint8_t token_count = 0;
        char *token = strtok(line, " ,\t\n");
        const char *tokens[MAX_LINE_TOKENS];
        while (token != NULL && token_count < MAX_LINE_TOKENS)
        {
            tokens[token_count++] = token;
            token = strtok(NULL, " ,\t\n");
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
                fclose(input);
                fclose(output);
                return 1;
            }

            Operand op1 = {0}, op2 = {0};
            if (analyzeOperand(tokens[1], &op1) != 0 || analyzeOperand(tokens[2], &op2) != 0)
            {
                fprintf(stderr, "Error: invalid operand(s) for 'mov'\n");
                fclose(input);
                fclose(output);
                return 1;
            }

            // validate operand combinations for MOV
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
                uint8_t instByte1 = 0, instByte2 = 0;
                // -------------------------------
                // register-to-register Instruction Byte 1:
                // -------------------------------
                // Format: 100010DW
                // - 6-bit opcode for MOV
                // - D = Direction bit (0: REG is source, R/M is dest)
                // - W = Word bit (0: 8-bit, 1: 16-bit)
                //
                // NASM defaults to D = 0 for register-to-register MOVs,
                // so we follow that for consistency.
                // This means, in the second byte: REG field = source, R/M field = destination.
                instByte1 |= 0x88; // MOV opcode for register-to-register (D = 0, W = 0 by default)
                if (op1.reg->size == 16)
                {
                    instByte1 |= 0x01; // W = 1 -> 16-bit operation
                }
                // -------------------------------
                // register-to-register Instruction Byte 2:
                // -------------------------------
                // Format: MOD REG R/M
                // - MOD  = MOD field: 11 -> register-to-register mode
                // - REG  = source register (since D = 0)
                // - R/M  = destination register
                instByte2 |= 0xC0;                 // MOD = 11 (register-to-register mode)
                instByte2 |= (op2.reg->mask << 3); // REG field (source reg, bits 5–3)
                instByte2 |= op1.reg->mask;        // R/M field (dest reg, bits 2–0)

                uint8_t bytes[2] = {instByte1, instByte2};
                fwrite(bytes, 1, 2, output);
            }
            // 2) immediate-to-register MOV
            else if (op1.type == OP_REG && op2.type == OP_IMM)
            {
                if (op2.imm->size > op1.reg->size)
                {
                    fprintf(stderr, "Error: immediate value too large for 8-bit register\n");
                    free(op2.imm); // no need to free here really, program is exiting but still leaving this in for explicit free
                    fclose(input);
                    fclose(output);
                    return 1;
                }
                uint8_t bytes[3]; // at most 3 bytes
                size_t count = 2;

                bytes[0] = 0xB0 | op1.reg->mask;
                bytes[1] = op2.imm->val; // Low byte
                if (op1.reg->size == 16)
                {
                    bytes[0] |= 0x08;                   // Set W=1
                    bytes[count++] = op2.imm->val >> 8; // High byte
                }
                fwrite(bytes, 1, count, output);
            }
            break;

        case MNEMONIC_INVALID:
            fprintf(stderr, "Unknown instruction in '%s', line %d: '%s'\n", input_filename, lineno, tokens[0]);
            fclose(input);
            fclose(output);
            return 1;

        default:
            fprintf(stderr, "Unimplemented mnemonic '%s'\n", tokens[0]);
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

MnemonicType getMnemonicType(const char *mnemonic)
{
    if (strcmp(mnemonic, "mov") == 0)
        return MNEMONIC_MOV;
    return MNEMONIC_INVALID;
}

int analyzeOperand(const char *op, Operand *out)
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
        out->type = OP_MEM;
        return 0;
    }

    // Try to parse signed decimal immediate
    errno = 0;
    char *end;
    long val = strtol(op, &end, 10);
    if (*end != '\0' || errno != 0)
    {
        return 1;
    }

    if (val < -65536 || val > 65535)
    {
        return 1;
    }

    Immediate *imm = malloc(sizeof(Immediate));
    if (!imm)
    {
        perror("malloc immediate");
        return 1;
    }

    imm->val = (uint16_t)val;
    imm->size = (val >= -256 && val <= 255) ? 8 : 16;

    out->type = OP_IMM;
    out->imm = imm;

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