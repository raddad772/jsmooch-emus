//
// Created by . on 6/12/25.
//

#ifndef JSMOOCH_EMUS_HUC6280_H
#define JSMOOCH_EMUS_HUC6280_H
#include "helpers/int.h"

union HUC6280_P {
    struct {

    };
    u8 u;
};

struct HUC6280_regs {
    union HUC6280_P P;
    u32 A, X, Y;
    u32 SP, PC;
};

struct HUC6280_pins {

};

struct HUC6280 {
    struct HUC6280_regs *regs;
    struct HUC6280_pins *pins;
};

#endif //JSMOOCH_EMUS_HUC6280_H
