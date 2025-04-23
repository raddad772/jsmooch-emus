//
// Created by . on 4/23/25.
//

#ifndef JSMOOCH_EMUS_SNES_APU_H
#define JSMOOCH_EMUS_SNES_APU_H

#include "component/cpu/spc700/spc700.h"

struct SNES_APU {
    struct SPC700 cpu;
};

struct SNES;
void SNES_APU_init(struct SNES *);
void SNES_APU_delete(struct SNES *);
void SNES_APU_reset(struct SNES *);
void SNES_APU_schedule_first(struct SNES *);
u32 SNES_APU_read(struct SNES *, u32 addr, u32 old, u32 has_effect);
void SNES_APU_write(struct SNES *, u32 addr, u32 val);

#endif //JSMOOCH_EMUS_SNES_APU_H
