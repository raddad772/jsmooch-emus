//
// Created by . on 7/25/24.
//

#ifndef JSMOOCH_EMUS_VIA6522_H
#define JSMOOCH_EMUS_VIA6522_H

#include "helpers/int.h"

struct via6522_line_write {
    void *ptr;
    u8 index;
    void (*write_line)(void *ptr, u8 index, u8 new_val);
};

enum
{
    VIAr_PB = 0,
    VIAr_PA = 1,
    VIAr_DDRB = 2,
    VIAr_DDRA = 3,
    VIAr_T1CL = 4,
    VIAr_T1CH = 5,
    VIAr_T1LL = 6,
    VIAr_T1LH = 7,
    VIAr_T2CL = 8,
    VIAr_T2CH = 9,
    VIAr_SR = 10,
    VIAr_ACR = 11,
    VIAr_PCR = 12,
    VIAr_IFR = 13,
    VIAr_IER = 14,
    VIAr_PANH = 15
};
struct via6522 {
    struct {
        u8 regA, regB;
    } io;

    struct {
        u64 *cycles;
        u64 dummy_cycles;
    } trace;
};

void via6522_init(via6522*, u64* trace_cycles);
void via6522_delete(via6522*);
void via6522_write(via6522*, u8 addr, u8 val);
u8 via6522_read(via6522*, u8 addr, u8 old);

#endif //JSMOOCH_EMUS_VIA6522_H
