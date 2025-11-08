
#include "helpers/int.h"

#ifndef JSMOOCH_LIB_M68K_GENTEST_ITEM_H
#define JSMOOCH_LIB_M68K_GENTEST_ITEM_H

struct m68k_gentest_item {
    char name[50];
    char full_name[50];
    u32 num_opcodes;
    u32 *opcodes;
};

#define M68K_NUM_GENTEST_ITEMS 125
extern struct m68k_gentest_item m68k_gentests[M68K_NUM_GENTEST_ITEMS];

#endif
