//
// Created by . on 1/19/25.
//

#ifndef JSMOOCH_EMUS_NDS_CP15_H
#define JSMOOCH_EMUS_NDS_CP15_H

#include "helpers/int.h"

#define ITCM_SIZE 0x8000
#define DTCM_SIZE 0x4000


struct ARM946ES;
u32 NDS_CP_read(ARM946ES *, u32 num, u32 opcode, u32 Cn, u32 Cm, u32 CP);
void NDS_CP_write(ARM946ES *, u32 num, u32 opcode, u32 Cn, u32 Cm, u32 CP, u32 val);
void NDS_CP_init(ARM946ES *);
void NDS_CP_reset(ARM946ES *);

#endif //JSMOOCH_EMUS_NDS_CP15_H
