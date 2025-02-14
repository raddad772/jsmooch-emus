//
// Created by . on 2/11/25.
//

#ifndef JSMOOCH_EMUS_GTE_H
#define JSMOOCH_EMUS_GTE_H

#include "helpers/int.h"

enum gteMatrix {
    GTE_Rotation = 0,
    GTE_Light = 1,
    GTE_Color = 2,
    GTE_Invalid = 3,
};

enum gteControlVector {
    GTE_Translation = 0,
    GTE_BackgroundColor = 1,
    GTE_FarColor = 2,
    GTE_Zero = 3
};

struct gte_cmd {
    u8 shift;
    u32 clamp_negative;
    enum gteMatrix matrix;
    u8 vector_mul;
    enum gteControlVector vector_add;
};


struct R3000;
struct R3000_GTE {
    struct gte_cmd config, config0;
    u32 op_going;
    u32 op_kind; // opcode
    u64 clock_start;
    u64 clock_end;
    i32 ofx, ofy;
    u16 h;
    i16 dqa;
    i32 dqb;
    i16 zsf3, zsf4;
    u32 cnt;
    u32 flags;
    i32 mac[4];
    u16 otz;
    u8 rgb[4];
    i16 ir[4];
    u16 z_fifo[4];
    u32 lzcs;
    u8 lzcr;
    u32 reg_23;

    i16 matrices[4][3][3];
    i32 control_vectors[4][3];
    i16 v[4][3];

    i16 xy_fifo[4][2];
    u8 rgb_fifo[3][4];

    i64 cycle_count;
};


void GTE_init(struct R3000_GTE *);
void GTE_command(struct R3000_GTE *this, u32 opcode, u64 current_clock);
void GTE_write_reg(struct R3000_GTE *, u32 num, u32 val);
u32 GTE_read_reg(struct R3000_GTE *, u32 num);

#endif //JSMOOCH_EMUS_GTE_H
