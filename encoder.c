#include <stdio.h>
#include <string.h>

#include "encoder.h"

static inline uint8_t get_reg_to_reg_opcode(MnemonicType mnemtype);
static inline uint8_t get_imm_to_acc_opcode(MnemonicType mnemtype);
static inline uint8_t get_opext(MnemonicType mnemtype);

int encode_instruction(Instruction *inst, uint8_t *buffer, size_t *out_size, size_t lineno)
{
    switch (inst->mnem)
    {
    case T_MOV:
        if (inst->op1.opType == OP_REG && inst->op2.opType == OP_REG)
        {
            uint8_t opcode = 0x88; // 10001000
            uint8_t dbit = 0;      // NASM style: r/m ← reg
            uint8_t wbit = (inst->op1.size == SZ_WORD) ? 1 : 0;
            buffer[0] = opcode | dbit | wbit;

            uint8_t mod = 0xC0;                   // 11000000 reg-to-reg
            uint8_t reg = inst->op2.reg.reg_code; // source  in REG field
            uint8_t rm = inst->op1.reg.reg_code;  // dest    in R/M field
            buffer[1] = mod | (reg << 3) | rm;

            *out_size = 2;
            return 0;
        }
        else if (inst->op1.opType == OP_REG && inst->op2.opType == OP_IMM)
        {
            uint8_t opcode = 0xB0; // 10110000 imm-to-reg
            uint8_t reg = inst->op1.reg.reg_code;
            uint16_t imm = inst->op2.imm.value;

            if (inst->op1.size == SZ_BYTE)
            {
                uint8_t wbit = 0;

                buffer[0] = opcode | wbit | reg;
                buffer[1] = (uint8_t)imm;

                *out_size = 2;
                return 0;
            }
            else
            {
                uint8_t wbit = 1;
                buffer[0] = opcode | (wbit << 3) | reg;
                buffer[1] = (uint8_t)imm;        // low byte
                buffer[2] = (uint8_t)(imm >> 8); // high byte

                *out_size = 3;
                return 0;
            }
        }
        else if ((inst->op1.opType == OP_REG && inst->op2.opType == OP_MEM) ||
                 (inst->op1.opType == OP_MEM && inst->op2.opType == OP_REG))
        {
            const Operand *regop = (inst->op1.opType == OP_REG) ? &inst->op1 : &inst->op2;
            bool reg_is_dest = (inst->op1.opType == OP_REG);

            const Operand *memop = (inst->op1.opType == OP_MEM) ? &inst->op1 : &inst->op2;
            bool is_direct = memop->mem.base_reg == NULL;

            bool is_accumulator = regop->reg.reg_code == 0 && (regop->size == SZ_BYTE || regop->size == SZ_WORD);

            uint8_t wbit = (regop->size == SZ_WORD) ? 1 : 0;
            if (is_accumulator && is_direct)
            {
                uint8_t opcode = 0xA2; // 10100010 acc-to-mem
                if (reg_is_dest)
                {
                    opcode = 0xA0; // 10100000 mem-to-acc
                }
                buffer[0] = opcode | wbit;

                if (wbit == 0)
                {
                    buffer[1] = (uint8_t)memop->mem.disp_value;
                    *out_size = 2;
                }
                else
                {
                    buffer[1] = (uint8_t)memop->mem.disp_value;
                    buffer[2] = (uint8_t)(memop->mem.disp_value >> 8);
                    *out_size = 3;
                }

                return 0;
            }

            uint8_t opcode = 0x88; // 10001000
            uint8_t dbit = reg_is_dest ? 1 : 0;

            buffer[0] = opcode | (dbit << 1) | wbit;

            uint8_t mod = 0;
            if (is_direct)
            {
                mod = 0;
            }
            else
            {
                switch (memop->mem.disp_size)
                {
                case SZ_NONE:
                    mod = 0;
                    break;
                case SZ_BYTE:
                    mod = 0x01;
                    break;
                case SZ_WORD:
                    mod = 0x02;
                    break;
                }
            }
            uint8_t reg = regop->reg.reg_code;
            uint8_t rm = memop->mem.rm_code;

            buffer[1] = (mod << 6) | (reg << 3) | rm;
            *out_size = 2;

            if (memop->mem.disp_size == SZ_BYTE)
            {
                buffer[2] = (uint8_t)memop->mem.disp_value;
                *out_size = 3;
            }
            else if (memop->mem.disp_size == SZ_WORD)
            {
                buffer[2] = (uint8_t)memop->mem.disp_value;
                buffer[3] = (uint8_t)(memop->mem.disp_value >> 8);
                *out_size = 4;
            }
            return 0;
        }
        else if (inst->op1.opType == OP_MEM && inst->op2.opType == OP_IMM)
        {
            uint8_t opcode = 0xC6; // 11000110 imm-to-mem
            uint8_t wbit = (inst->op1.size == SZ_WORD) ? 1 : 0;

            buffer[0] = opcode | wbit;

            bool is_direct = (inst->op1.mem.base_reg == NULL && inst->op1.mem.index_reg == NULL);
            uint8_t mod = 0;
            if (is_direct)
            {
                mod = 0;
            }
            else
            {
                switch (inst->op1.mem.disp_size)
                {
                case SZ_NONE:
                    mod = 0;
                    break;
                case SZ_BYTE:
                    mod = 0x01;
                    break;
                case SZ_WORD:
                    mod = 0x02;
                    break;
                }
            }
            uint8_t reg = 0;
            uint8_t rm = inst->op1.mem.rm_code;

            buffer[1] = (mod << 6) | (reg << 3) | rm;
            *out_size = 2;

            if (inst->op1.mem.disp_size == SZ_BYTE)
            {
                buffer[(*out_size)++] = (uint8_t)inst->op1.mem.disp_value;
            }
            else if (inst->op1.mem.disp_size == SZ_WORD)
            {
                buffer[(*out_size)++] = (uint8_t)inst->op1.mem.disp_value;
                buffer[(*out_size)++] = (uint8_t)(inst->op1.mem.disp_value >> 8);
            }

            if (wbit == 0)
            {
                buffer[(*out_size)++] = (uint8_t)inst->op2.imm.value;
            }
            else
            {
                buffer[(*out_size)++] = (uint8_t)inst->op2.imm.value;
                buffer[(*out_size)++] = (uint8_t)(inst->op2.imm.value >> 8);
            }
            return 0;
        }
    case T_ADD:
    case T_SUB:
    case T_CMP:
        if (inst->op1.opType == OP_REG && inst->op2.opType == OP_REG)
        {
            uint8_t opcode = get_reg_to_reg_opcode(inst->mnem);
            uint8_t dbit = 0; // NASM style: r/m ← reg
            uint8_t wbit = (inst->op1.size == SZ_WORD) ? 1 : 0;
            buffer[0] = opcode | dbit | wbit;

            uint8_t mod = 0xC0;                   // 11000000 reg-to-reg
            uint8_t reg = inst->op2.reg.reg_code; // source  in REG field
            uint8_t rm = inst->op1.reg.reg_code;  // dest    in R/M field
            buffer[1] = mod | (reg << 3) | rm;

            *out_size = 2;
            return 0;
        }
        else if (inst->op1.opType == OP_REG && inst->op2.opType == OP_IMM)
        {
            bool is_accumulator = inst->op1.reg.reg_code == 0 && (inst->op1.size == SZ_BYTE || inst->op1.size == SZ_WORD);
            int16_t imm = inst->op2.imm.value;

            if (is_accumulator)
            {
                uint8_t opcode = get_imm_to_acc_opcode(inst->mnem);
                uint8_t wbit = (inst->op1.size == SZ_WORD) ? 1 : 0;
                buffer[0] = opcode | wbit;

                if (wbit == 0)
                {
                    buffer[1] = (uint8_t)imm;
                    *out_size = 2;
                }
                else
                {
                    buffer[1] = (uint8_t)imm;        // low byte
                    buffer[2] = (uint8_t)(imm >> 8); // high byte
                    *out_size = 3;
                }

                return 0;
            }

            bool w = (inst->op1.size == SZ_WORD);
            bool fit = (imm >= -128 && imm <= 127);

            uint8_t opcode = 0x80;
            uint8_t sbit = (w && fit) ? 1 : 0;
            uint8_t wbit = w ? 1 : 0;
            buffer[0] = opcode | (sbit << 1) | wbit;

            uint8_t mod = 0xC0; // 11000000 reg direct
            uint8_t opext = get_opext(inst->mnem);
            uint8_t reg = inst->op1.reg.reg_code;
            buffer[1] = mod | (opext << 3) | reg;

            if (!w || (w && fit))
            {
                buffer[2] = (uint8_t)imm;
                *out_size = 3;
            }
            else
            {
                buffer[2] = (uint8_t)imm;        // low byte
                buffer[3] = (uint8_t)(imm >> 8); // high byte
                *out_size = 4;
            }
            return 0;
        }
        else if ((inst->op1.opType == OP_REG && inst->op2.opType == OP_MEM) ||
                 (inst->op1.opType == OP_MEM && inst->op2.opType == OP_REG))
        {
            const Operand *regop = (inst->op1.opType == OP_REG) ? &inst->op1 : &inst->op2;
            bool reg_is_dest = (inst->op1.opType == OP_REG);

            const Operand *memop = (inst->op1.opType == OP_MEM) ? &inst->op1 : &inst->op2;
            bool is_direct = memop->mem.base_reg == NULL;

            uint8_t opcode = get_reg_to_reg_opcode(inst->mnem);
            uint8_t dbit = reg_is_dest ? 1 : 0;
            uint8_t wbit = (regop->size == SZ_WORD) ? 1 : 0;
            buffer[0] = opcode | (dbit << 1) | wbit;

            uint8_t mod = 0;
            if (is_direct)
            {
                mod = 0;
            }
            else
            {
                switch (memop->mem.disp_size)
                {
                case SZ_NONE:
                    mod = 0;
                    break;
                case SZ_BYTE:
                    mod = 0x01;
                    break;
                case SZ_WORD:
                    mod = 0x02;
                    break;
                }
            }
            uint8_t reg = regop->reg.reg_code;
            uint8_t rm = memop->mem.rm_code;
            buffer[1] = (mod << 6) | (reg << 3) | rm;
            *out_size = 2;

            if (memop->mem.disp_size == SZ_BYTE)
            {
                buffer[2] = (uint8_t)memop->mem.disp_value;
                *out_size = 3;
            }
            else if (memop->mem.disp_size == SZ_WORD)
            {
                buffer[2] = (uint8_t)memop->mem.disp_value;
                buffer[3] = (uint8_t)(memop->mem.disp_value >> 8);
                *out_size = 4;
            }
            return 0;
        }
        else if (inst->op1.opType == OP_MEM && inst->op2.opType == OP_IMM)
        {
            int16_t imm = inst->op2.imm.value;
            bool w = (inst->op1.size == SZ_WORD);
            bool fit = (imm >= -128 && imm <= 127);

            uint8_t opcode = 0x80;
            uint8_t sbit = (w && fit) ? 1 : 0;
            uint8_t wbit = w ? 1 : 0;
            buffer[0] = opcode | (sbit << 1) | wbit;

            bool is_direct = inst->op1.mem.base_reg == NULL;
            uint8_t mod = 0;
            if (is_direct)
            {
                mod = 0;
            }
            else
            {
                switch (inst->op1.mem.disp_size)
                {
                case SZ_NONE:
                    mod = 0;
                    break;
                case SZ_BYTE:
                    mod = 0x01;
                    break;
                case SZ_WORD:
                    mod = 0x02;
                    break;
                }
            }
            uint8_t opext = get_opext(inst->mnem);
            uint8_t mem = inst->op1.mem.rm_code;
            buffer[1] = (mod << 6) | (opext << 3) | mem;
            *out_size = 2;

            if (inst->op1.mem.disp_size == SZ_BYTE)
            {
                buffer[(*out_size)++] = (uint8_t)inst->op1.mem.disp_value;
            }
            else if (inst->op1.mem.disp_size == SZ_WORD)
            {
                buffer[(*out_size)++] = (uint8_t)inst->op1.mem.disp_value;
                buffer[(*out_size)++] = (uint8_t)(inst->op1.mem.disp_value >> 8);
            }

            if (!w || (w && fit))
            {
                buffer[(*out_size)++] = (uint8_t)imm;
            }
            else
            {
                buffer[(*out_size)++] = (uint8_t)imm;
                buffer[(*out_size)++] = (uint8_t)(imm >> 8);
            }
            return 0;
        }
    }

    fprintf(stderr, "Error on line %zu: encoding of that instruction is not supported for now\n", lineno);
    return 1;
}

static inline uint8_t get_reg_to_reg_opcode(MnemonicType mnemtype)
{
    switch (mnemtype)
    {
    case T_ADD:
        return 0;
    case T_SUB:
        return 0x28;
    case T_CMP:
        return 0x38;
    }
}

static inline uint8_t get_imm_to_acc_opcode(MnemonicType mnemtype)
{
    switch (mnemtype)
    {
    case T_ADD:
        return 0x4;
    case T_SUB:
        return 0x2C;
    case T_CMP:
        return 0x3C;
    }
}

static inline uint8_t get_opext(MnemonicType mnemtype)
{
    switch (mnemtype)
    {
    case T_ADD:
        return 0;
    case T_SUB:
        return 0x5;
    case T_CMP:
        return 0x7;
    }
}