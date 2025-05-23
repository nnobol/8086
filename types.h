#include <stdint.h>

#define MAX_LINE_SIZE 256
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
    uint8_t mod;
    uint16_t dis;
    Register *reg;
    Immediate *imm;
} Memory;

typedef struct
{
    OperandType type;
    Register *reg;  // if type == OP_REG
    Immediate *imm; // if type == OP_IMM
    Memory *mem;    // if type == OP_MEM
} Operand;

typedef enum
{
    MNEMONIC_INVALID = -1,
    MNEMONIC_MOV
} MnemonicType;

typedef struct
{
    char reg1[3];
    char reg2[3];
    uint8_t rmCode;
} AddressEncoding;

void lineToLower(char *line);
int tokenizeLine(char *line, char *tokens[], uint8_t *token_count, int *lineno);
MnemonicType getMnemonicType(const char *mnemonic);
int parseOperand(const char *token, Operand *out, int *lineno);
int analyzeMemOperand(const char *token, Memory *out, int *lineno);
int isPureNumericExpression(const char *expr, int *value, int *lineno);
int validateOpCombination(const Operand *op1, const Operand *op2);

void freeTokens(char *tokens[], int token_count);