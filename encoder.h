#ifndef ENCODER_H
#define ENCODER_H

#include "parser.h" // for Instruction

int encode_instruction(Instruction *inst, uint8_t *buffer, size_t *out_size, size_t lineno);

#endif