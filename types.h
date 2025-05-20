#include <stdint.h>

#define MAX_LINE_TOKENS 8

typedef enum
{
    OP_INVALID = -1,
    OP_REG,
    OP_IMM,
    OP_MEM
} OperandType;

typedef struct
{
    char name[3];
    uint8_t size;
    uint8_t mask;
} Register;

typedef struct
{
    uint8_t size;
    uint16_t val;
} Immediate;

typedef struct
{
    OperandType type;
    const Register *reg; // if type == OP_REG
    Immediate *imm;      // if type == OP_IMM
} Operand;

typedef enum
{
    MNEMONIC_INVALID = -1,
    MNEMONIC_MOV
} MnemonicType;

void lineToLower(char *line);
MnemonicType getMnemonicType(const char *mnemonic);
int analyzeOperand(const char *token, Operand *out);
int validateOpCombination(const Operand *op1, const Operand *op2);