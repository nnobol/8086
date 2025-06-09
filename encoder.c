#include <stdio.h>
#include <string.h>

#include "encoder.h"

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

            uint8_t wbit = (regop->size == SZ_WORD) ? 1 : 0;
            if ((regop->reg.reg_code == 0 && (regop->size == SZ_BYTE || regop->size == SZ_WORD)) && is_direct)
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
    }

    fprintf(stderr, "Error on line %zu: encoding of that instruction is not supported for now\n", lineno);
    return 1;
}