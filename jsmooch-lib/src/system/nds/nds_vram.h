//
// Created by . on 1/19/25.
//

#ifndef JSMOOCH_EMUS_NDS_VRAM_H
#define JSMOOCH_EMUS_NDS_VRAM_H

#include "helpers/int.h"

#define NVA 0
#define NVB 1
#define NVC 2
#define NVD 3
#define NVE 4
#define NVF 5
#define NVG 6
#define NVH 7
#define NVI 8

struct NDS;
void NDS_VRAM_resetup_banks(NDS *);
u32 NDS_VRAM_tex_read(NDS *, u32 addr, u32 sz);
u32 NDS_VRAM_pal_read(NDS *, u32 addr, u32 sz);
#endif //JSMOOCH_EMUS_NDS_VRAM_H