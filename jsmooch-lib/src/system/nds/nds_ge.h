//
// Created by . on 3/9/25.
//

#ifndef JSMOOCH_EMUS_NDS_GE_H
#define JSMOOCH_EMUS_NDS_GE_H

#include "helpers/int.h"

enum NDS_GE_cmds {
    NDS_GE_CMD_NOP = 0x00,
    NDS_GE_CMD_MTX_MODE = 0x10,
    NDS_GE_CMD_MTX_PUSH = 0x11,
    NDS_GE_CMD_MTX_POP = 0x12,
    NDS_GE_CMD_MTX_STORE = 0x13,
    NDS_GE_CMD_MTX_RESTORE = 0x14,
    NDS_GE_CMD_MTX_IDENTITY = 0x15,
    NDS_GE_CMD_MTX_LOAD_4x4 = 0x16,
    NDS_GE_CMD_MTX_LOAD_4x3 = 0x17,
    NDS_GE_CMD_MTX_MULT_4x4 = 0x18,
    NDS_GE_CMD_MTX_MULT_4x3 = 0x19,
    NDS_GE_CMD_MTX_MULT_3x3 = 0x1A,
    NDS_GE_CMD_MTX_SCALE = 0x1B,
    NDS_GE_CMD_MTX_TRANS = 0x1C,
    NDS_GE_CMD_COLOR = 0x20,
    NDS_GE_CMD_NORMAL = 0x21,
    NDS_GE_CMD_TEXCOORD = 0x22,
    NDS_GE_CMD_VTX_16 = 0x23,
    NDS_GE_CMD_VTX_10 = 0x24,
    NDS_GE_CMD_VTX_XY = 0x25,
    NDS_GE_CMD_VTX_XZ = 0x26,
    NDS_GE_CMD_VTX_YZ = 0x27,
    NDS_GE_CMD_VTX_DIFF = 0x28,
    NDS_GE_CMD_POLYGON_ATTR = 0x29,
    NDS_GE_CMD_TEXIMAGE_PARAM = 0x2A,
    NDS_GE_CMD_PLTT_BASE = 0x2B,
    NDS_GE_CMD_DIFF_AMB = 0x30,
    NDS_GE_CMD_SPE_EMI = 0x31,
    NDS_GE_CMD_LIGHT_VECTOR = 0x32,
    NDS_GE_CMD_LIGHT_COLOR = 0x33,
    NDS_GE_CMD_SHININESS = 0x34,
    NDS_GE_CMD_BEGIN_VTXS = 0x40,
    NDS_GE_CMD_END_VTXS = 0x41,
    NDS_GE_CMD_SWAP_BUFFERS = 0x50,
    NDS_GE_CMD_VIEWPORT = 0x60,
    NDS_GE_CMD_BOX_TEST = 0x70,
    NDS_GE_CMD_POS_TEST = 0x71,
    NDS_GE_CMD_VEC_TEST = 0x72,
};


struct NDS_GE_FIFO_ENTRY {
    u8 cmd;
    u32 param;
};

struct NDS_GE_matrix {


};

struct NDS_GE_BUFFERS {
    u8 polygon[1024 * 104];
    u8 vertex[1024 * 144];
};

struct NDS_GE {
    struct NDS_GE_BUFFERS buffers[2];
    u32 ge_buffer;

    /*
     commands can be written either
     */

    struct {
        struct NDS_GE_FIFO_ENTRY items[256];
        u32 head, tail, len;
    } fifo;
    // when PIPE goes down to <3, data is pulled from FIFO
    // when FIFO reaches <112, DMA starts if it is there to do 112 words
    struct {
        struct NDS_GE_FIFO_ENTRY items[4];
        u32 len;
    } pipe;

    struct {
        struct NDS_GE_matrix stack[50];
    } matrix;

    struct {
        union {
            struct {
                u32 texture_mapping : 1;
                u32 attr_shading : 1;
                u32 alpha_test : 1;
                u32 alpha_blend : 1;
                u32 anti_alias : 1;
                u32 edge_mark : 1;
                u32 fog_color_alpha_mode : 1;
                u32 fog_enable : 1;
                u32 fog_depth_shift : 4;
                u32 rdlines_underflow : 1;
                u32 ram_overflow : 1;
                u32 rear_plane_mode : 1;
            };
            u32 u;
        } DISP3DCNT;
    } io;
};

struct NDS;
void NDS_GE_init(struct NDS *);

#endif //JSMOOCH_EMUS_NDS_GE_H
