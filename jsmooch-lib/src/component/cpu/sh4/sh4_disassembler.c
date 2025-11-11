//
// Created by Dave on 2/13/2024.
//

#include "stdio.h"
#include "string.h"
#include "sh4_disassembler.h"

void dasm_support_init(dasm_support* this)
{
    *this = (dasm_support) {
        .read8 = NULL,
        .read16 = NULL,
        .read32 = NULL
    };
    buf_init(&this->strout);
}

void dasm_support_delete(dasm_support* this)
{
    buf_delete(&this->strout);
}

// We have opcode like 0xBEEF
// 1011111011101111
//         mmmmnnnn

// we can say it is like
// 10111101mmnnnnnn
// We want n to be the first 6 bits here, m the next 2, and the mask to be the rest
// We need to process the string from left to right

