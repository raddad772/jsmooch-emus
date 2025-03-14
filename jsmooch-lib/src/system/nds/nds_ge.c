//
// Created by . on 3/9/25.
//

#include <stdlib.h>
#include <string.h>
#include "nds_ge.h"
#include "nds_bus.h"
#include "nds_irq.h"
#include "nds_regs.h"
#include "nds_dma.h"
#include "helpers/multisize_memaccess.c"

#define printfcd(...) (void)0

static u32 tbl_num_params[0xFF];
static i32 tbl_num_cycles[0xFF];
static u32 tbl_cmd_good[0xFF];

static float vtx_to_float(i32 v)
{
    return ((float)v) / 4096.0f;
}

static void pprint_matrix(const char *n, i32 *s) {
    printfcd("\nMatrix %s", n);
    for (u32 y = 0; y < 16; y+=4) {
        printfcd("\n%f %f %f %f", vtx_to_float(s[y+0]), vtx_to_float(s[y+1]), vtx_to_float((s[y+2])), vtx_to_float(s[y+3]));
    }
}

#define VTX_ROOT this->ge.params.vtx.root
#define VTX_ALIST this->ge.params.vtx.alloc_list
#define MS_COORD_DIR_PTR this->ge.matrices.stacks.coordinate_direction_ptr
#define MS_TEXTURE_PTR this->ge.matrices.stacks.texture_ptr
#define MS_PROJECTION_PTR this->ge.matrices.stacks.projection_ptr
#define MS_COORD this->ge.matrices.stacks.coordinate
#define MS_TEXTURE this->ge.matrices.stacks.texture
#define MS_PROJECTION this->ge.matrices.stacks.projection
#define MS_DIR this->ge.matrices.stacks.direction
#define M_COORD this->ge.matrices.coordinate
#define M_TEXTURE this->ge.matrices.texture
#define M_PROJECTION this->ge.matrices.projection
#define M_DIR this->ge.matrices.direction
#define M_CLIP this->ge.matrices.clip
#define M_SZ sizeof(struct NDS_GE_matrix)
#define GXSTAT this->ge.io.GXSTAT

static void identity_matrix(i32 *m)
{
    memset(m, 0, sizeof(i32)*16);
    m[0] = m[5] = m[10] = m[15] = 1 << 12;
}


void NDS_GE_init(struct NDS *this) {
    this->ge.ge_has_buffer = 0;
    for (u32 i = 0; i < 0xFF; i++) {
        tbl_num_params[i] = 0;
        tbl_num_cycles[i] = 0;
        tbl_cmd_good[i] = 0;
    }

#define SCMD(cmd_id, np, ncyc) tbl_num_params[NDS_GE_CMD_##cmd_id] = np; tbl_num_cycles[NDS_GE_CMD_##cmd_id] = ncyc; tbl_cmd_good[NDS_GE_CMD_##cmd_id] = 1
    SCMD(MTX_MODE, 1, 1);
    SCMD(MTX_PUSH, 0, 17);
    SCMD(MTX_POP, 1, 36);
    SCMD(MTX_STORE, 1, 17);
    SCMD(MTX_RESTORE, 1, 36);
    SCMD(MTX_IDENTITY, 0, 19);
    SCMD(MTX_LOAD_4x4, 16, 34);
    SCMD(MTX_LOAD_4x3, 12, 30);
    SCMD(MTX_MULT_4x4, 16, -1);
    SCMD(MTX_MULT_4x3, 12, -1);
    SCMD(MTX_MULT_3x3, 9, -1);
    SCMD(MTX_SCALE, 3, 22);
    SCMD(MTX_TRANS, 3, 22);
    SCMD(COLOR, 1, 1);
    SCMD(NORMAL, 1, -1);
    SCMD(TEXCOORD, 1, 1);
    SCMD(VTX_16, 2, 9);
    SCMD(VTX_10, 1, 8);
    SCMD(VTX_XY, 1, 8);
    SCMD(VTX_XZ, 1, 8);
    SCMD(VTX_YZ, 1, 8);
    SCMD(VTX_DIFF, 1, 8);
    SCMD(POLYGON_ATTR, 1, 1);
    SCMD(TEXIMAGE_PARAM, 1, 1);
    SCMD(SWAP_BUFFERS, 0, 1);
    SCMD(PLTT_BASE, 1, 1);
    SCMD(DIFF_AMB, 1, 4);
    SCMD(SPE_EMI, 1, 4);
    SCMD(LIGHT_VECTOR, 1, 6);
    SCMD(LIGHT_COLOR, 1, 1);
    SCMD(SHININESS, 32, 32);
    SCMD(BEGIN_VTXS, 1, 1);
    SCMD(END_VTXS, 0, 1);
    SCMD(SWAP_BUFFERS, 1, 1);
    SCMD(VIEWPORT, 1, 1);
    SCMD(BOX_TEST, 3, 103);
    SCMD(POS_TEST, 2, 9);
    SCMD(VEC_TEST, 1, 5);
#undef SCMD

    NDS_GE_reset(this);
}

void NDS_GE_reset(struct NDS *this)
{
    // Load identity matrices everywhere
    identity_matrix(M_PROJECTION.m);
    identity_matrix(M_CLIP.m);
    identity_matrix(M_COORD.m);
    identity_matrix(M_TEXTURE.m);
    identity_matrix(M_DIR.m);
    memset(&this->ge.params.vtx.alloc_list, 0, sizeof(struct NDS_GE_ALLOC_LIST));
    this->ge.params.vtx.alloc_list.list_len = NDS_GE_VTX_LIST_MAX - 1;
    for (u32 i = 0; i < NDS_GE_VTX_LIST_MAX; i++) {
        this->ge.params.vtx.alloc_list.items[i] = &this->ge.params.vtx.alloc_list.pool[i];
    }
}

static void set_GXSTAT_fifo(struct NDS *this)
{
    u32 old_bit = 0;
    if (GXSTAT.cmd_fifo_irq_mode) {
        old_bit = NDS_GE_check_irq(this);
    }

    GXSTAT.cmd_fifo_len = this->ge.fifo.len > 255 ? 255 : this->ge.fifo.len;
    GXSTAT.cmd_fifo_less_than_half_full = this->ge.fifo.len < 128;
    GXSTAT.cmd_fifo_empty = this->ge.fifo.len == 0;

    if (GXSTAT.cmd_fifo_irq_mode) {
        u32 new_bit = NDS_GE_check_irq(this);
        if ((old_bit != new_bit) && new_bit)
            NDS_update_IF9(this, NDS_IRQ_GXFIFO);
    }
}

static void check_cpu_lock(struct NDS *this)
{
    this->ge.fifo.pausing_cpu = this->ge.fifo.len >= 256;
}

static u32 fifo_pop_full_cmd(struct NDS *this, u32 *cmd, u32 *data)
{
    if ((this->ge.pipe.len == 0) && (this->ge.fifo.len == 0)) {
        // Failure! We're fully empty!
        return 0;
    }

    u32 old_len = this->ge.fifo.len;

    // Only complete commands are pushed, so we can easily grab them out!
    if (this->ge.pipe.len > 0) {
        *cmd = this->ge.pipe.items[this->ge.pipe.head].cmd;
        data[0] = this->ge.pipe.items[this->ge.pipe.head].data;
        this->ge.pipe.head = (this->ge.pipe.head + 1) & 3;
        this->ge.pipe.len--;
    }
    else {
        *cmd = this->ge.fifo.items[this->ge.fifo.head].cmd;
        data[0] = this->ge.fifo.items[this->ge.fifo.head].data;
        this->ge.fifo.head = (this->ge.fifo.head + 1) & 511;
        this->ge.fifo.len--;
    }

    u32 num_params = tbl_num_params[*cmd];
    u32 params_left = num_params ? num_params - 1 : 0;
    u32 idx = 1;
    while(params_left != 0) {
        if (this->ge.pipe.len > 0) {
            data[idx] = this->ge.pipe.items[this->ge.pipe.head].data;
            this->ge.pipe.head = (this->ge.pipe.head + 1) & 3;
            this->ge.pipe.len--;
        }
        else {
            assert(this->ge.fifo.len > 0);
            data[idx] = this->ge.fifo.items[this->ge.fifo.head].data;
            this->ge.fifo.head = (this->ge.fifo.head + 1) & 511;
            this->ge.fifo.len--;
        }
        params_left--;
        idx++;
    }

    // Now refill the pipeline by popping from FIFO and pushing into pipe
    while ((this->ge.pipe.len < 4) && (this->ge.fifo.len > 0)) {
        this->ge.pipe.items[this->ge.pipe.tail].cmd = this->ge.fifo.items[this->ge.fifo.head].cmd;
        this->ge.pipe.items[this->ge.pipe.tail].data = this->ge.fifo.items[this->ge.fifo.head].data;
        this->ge.pipe.tail = (this->ge.pipe.tail + 1) & 3;
        this->ge.fifo.head = (this->ge.fifo.head + 1) & 511;
        this->ge.pipe.len++;
        this->ge.fifo.len--;
    }

    set_GXSTAT_fifo(this);
    check_cpu_lock(this);
    if (this->ge.fifo.len < 112) {
        NDS_trigger_dma9_if(this, NDS_DMA_GE_FIFO);
    }

    return 1;
}

static void fifo_push(struct NDS *this, u8 cmd, u32 data)
{
    // Determine to go to pipe or FIFO
    // Push there
    // Run DMA if FIFO less than half full
    // Update bitflags
    // Add to "FIFO done timing," reschedule event
    if ((this->ge.fifo.len == 0) && (this->ge.pipe.len < 4)) {
        this->ge.pipe.items[this->ge.pipe.tail].cmd = cmd;
        this->ge.pipe.items[this->ge.pipe.tail].data = data;
        this->ge.pipe.len++;
        this->ge.pipe.tail = (this->ge.pipe.tail + 1) & 3;
        return;
    }

    // OK add to actual FIFO
    this->ge.fifo.items[this->ge.fifo.tail].cmd = cmd;
    this->ge.fifo.items[this->ge.fifo.tail].data = data;
    this->ge.fifo.len++;
    this->ge.fifo.tail = (this->ge.fifo.tail + 1) & 511;

    set_GXSTAT_fifo(this);
    check_cpu_lock(this);
}

static i32 get_num_cycles(struct NDS *this, u32 cmd)
{
    i32 num_cycles = tbl_num_cycles[cmd];
    if (num_cycles == -1) { // Calculate number of cycles a bit differently
        i32 mtx_add = this->ge.io.MTX_MODE == 2 ? 30 : 0;
        switch(cmd) {
            case NDS_GE_CMD_MTX_MULT_4x4:
                return 35 + mtx_add;
            case NDS_GE_CMD_MTX_MULT_4x3:
                return 31 + mtx_add;
            case NDS_GE_CMD_MTX_MULT_3x3:
                return 28 + mtx_add;
            case NDS_GE_CMD_MTX_TRANS:
                return 22 + mtx_add;
            case NDS_GE_CMD_NORMAL:
                return 8 + this->ge.params.poly.current.num_lights;
            default:
            NOGOHERE;
        }
    }
    return num_cycles;
}

static void ge_handle_cmd(struct NDS *this);

#define DATA this->ge.cur_cmd.data

static void cmd_MTX_MODE(struct NDS *this)
{
    this->ge.io.MTX_MODE = DATA[0] & 3;
    printfcd("\nMTX_MODE(%d);", this->ge.io.MTX_MODE);
}

/*
 0  Projection Matrix - only one
 1  Position Matrix (aka Modelview Matrix) - ?
 2  Position & Vector Simultaneous Set mode (used for Light+VEC_TEST) ??
 3  Texture Matrix (see DS 3D Texture Coordinates chapter) - 1
 */
/*
Coordinate  = position matrix
Vector  = direction
 */

static void calculate_clip_matrix(struct NDS *this)
{
    // ClipMatrix = PositionMatrix * ProjectionMatrix
#define A M_COORD.m
#define B M_PROJECTION.m
    //printfcd("\n\n----CALCULATE CLIP MATRIX. COORD:");
    //pprint_matrix(A);
    //printfcd("\nPROJECTION:");
    //pprint_matrix(B);
    for (u32 x = 0; x < 4; x++) {
        for (u32 y = 0; y < 16; y+=4) {
            //cyx = ay1*b1x + ay2*b2x + ay3*b3x + ay4*b4x
            i64 out = ((i64)A[y+0] * (i64)B[x]) >> 12;
            out += ((i64)A[y+1] * (i64)B[4+x]) >> 12;
            out += ((i64)A[y+2] * (i64)B[8+x]) >> 12;
            out += ((i64)A[y+3] * (i64)B[12+x]) >> 12;
            this->ge.matrices.clip.m[x+y] = (i32)out;
        }
    }
#undef A
#undef B
    //printfcd("\nOUTPUT MATRIX:");
    //pprint_matrix( this->ge.matrices.clip.m);
}

static void cmd_MTX_PUSH(struct NDS *this)
{
    // ClipMatrix = PositionMatrix * ProjectionMatrix
    u32 old_ptr;
    switch(this->ge.io.MTX_MODE) {
        case 0:
            old_ptr = MS_PROJECTION_PTR;
            memcpy(&MS_PROJECTION[MS_PROJECTION_PTR], &M_PROJECTION, M_SZ);
            MS_PROJECTION_PTR = (MS_PROJECTION_PTR + 1) & 1;
            GXSTAT.matrix_stack_over_or_underflow_error |= MS_PROJECTION_PTR == 1;
            GXSTAT.projection_matrix_stack_level = MS_PROJECTION_PTR;
            printfcd("\nMTX_PUSH(PROJ, old:%d new:%d);", old_ptr, MS_PROJECTION_PTR);
            //printfcd("\nPushed projection matrix:");
            //pprint_matrix(M_PROJECTION.m);
            break;
        case 1:
        case 2:
            old_ptr = MS_COORD_DIR_PTR;

            memcpy(&MS_COORD[MS_COORD_DIR_PTR], &M_COORD, M_SZ);
            memcpy(&MS_DIR[MS_COORD_DIR_PTR], &M_DIR, M_SZ);
            MS_COORD_DIR_PTR = (MS_COORD_DIR_PTR + 1) & 63;
            GXSTAT.matrix_stack_over_or_underflow_error |= (MS_COORD_DIR_PTR & 31) == 31;
            GXSTAT.position_vector_matrix_stack_level = MS_COORD_DIR_PTR;
            printfcd("\nMTX_PUSH(COORD&DIR, old:%d new:%d);", old_ptr, MS_COORD_DIR_PTR);
            //printfcd("\nMTX_PUSH COORD&DIR old:%d new:%d", old_ptr, MS_COORD_DIR_PTR);
            //printfcd("\nPushed coord matrix:");
            //pprint_matrix(M_COORD.m);
            //printfcd("\nPushed dir matrix:");
            //pprint_matrix(M_DIR.m);
            break;
        case 3:
            old_ptr = MS_TEXTURE_PTR;
            memcpy(&MS_TEXTURE[MS_TEXTURE_PTR], &M_TEXTURE, M_SZ);
            MS_TEXTURE_PTR = (MS_TEXTURE_PTR + 1) & 1;
            GXSTAT.matrix_stack_over_or_underflow_error |= MS_TEXTURE_PTR == 1;
            //printfcd("\nPushed texture matrix:");
            //pprint_matrix(M_TEXTURE.m);
            // no texture matrix GXSTAT bits
            break;
    }
}

static void cmd_MTX_POP(struct NDS *this)
{
    i32 n = DATA[0];
    n = SIGNe6to32(n);
    u32 old_ptr;
    switch(this->ge.io.MTX_MODE) {
        case 0:
            old_ptr = MS_PROJECTION_PTR;
            MS_PROJECTION_PTR = (MS_PROJECTION_PTR - n) & 1;
            GXSTAT.matrix_stack_over_or_underflow_error |= MS_PROJECTION_PTR == 0;
            GXSTAT.projection_matrix_stack_level = MS_PROJECTION_PTR;
            memcpy(&M_PROJECTION, &MS_PROJECTION[MS_PROJECTION_PTR], M_SZ);
            printfcd("\nMTX_POP(PROJECTION, n:%d old:%d new:%d);", n, old_ptr, MS_PROJECTION_PTR);
            calculate_clip_matrix(this);
            break;
        case 1:
        case 2:
            old_ptr = MS_COORD_DIR_PTR;
            MS_COORD_DIR_PTR = (MS_COORD_DIR_PTR - n) & 63;
            GXSTAT.matrix_stack_over_or_underflow_error |= (MS_COORD_DIR_PTR & 31) == 0;
            GXSTAT.position_vector_matrix_stack_level = MS_COORD_DIR_PTR;
            memcpy(&M_COORD, &MS_COORD[MS_COORD_DIR_PTR], M_SZ);
            memcpy(&M_DIR, &MS_DIR[MS_COORD_DIR_PTR], M_SZ);
            printfcd("\nMTX_POP(COORD&DIR, n:%d old:%d new:%d);", n, old_ptr, MS_COORD_DIR_PTR);
            calculate_clip_matrix(this);
            break;
        case 3:
            old_ptr = MS_TEXTURE_PTR;
            MS_TEXTURE_PTR = (MS_TEXTURE_PTR - n) & 1;
            GXSTAT.matrix_stack_over_or_underflow_error |= MS_TEXTURE_PTR == 0;
            printfcd("\nMTX_POP(TEXTURE, n:%d old:%d new:%d);", n, old_ptr, MS_TEXTURE_PTR);
            memcpy(&M_TEXTURE, &MS_TEXTURE[MS_TEXTURE_PTR], M_SZ);
            // no texture matrix GXSTAT bits
            break;
    }
}

static void cmd_MTX_STORE(struct NDS *this)
{
    // bit 0..4 is stack address. copy stack[S] to stack[N]
    printfcd("\nMTX_STORE();");
    u32 n = DATA[0] & 0x1F;
    GXSTAT.matrix_stack_over_or_underflow_error |= n == 1;
    switch(this->ge.io.MTX_MODE) {
        case 0:
            memcpy(&MS_PROJECTION[0], &M_PROJECTION, M_SZ);
            calculate_clip_matrix(this);
            break;
        case 1:
        case 2:
            memcpy(&MS_DIR[n], &M_DIR, M_SZ);
            memcpy(&MS_COORD[n], &M_COORD, M_SZ);
            calculate_clip_matrix(this);
            break;
        case 3:
            memcpy(&MS_TEXTURE[n & 1], &M_TEXTURE, M_SZ);
            break;
    }
}

static void cmd_MTX_RESTORE(struct NDS *this)
{
    // bit 0..4 is stack address. copy stack[N] to stack[S]
    printfcd("\nMTX_RESTORE();");
    u32 n = DATA[0] & 0x1F;
    GXSTAT.matrix_stack_over_or_underflow_error |= n == 1;
    switch(this->ge.io.MTX_MODE) {
        case 0:
            memcpy(&M_PROJECTION, &MS_PROJECTION[0], M_SZ);
            calculate_clip_matrix(this);
            break;
        case 1:
        case 2:
            if (n != MS_COORD_DIR_PTR) {
                memcpy(&M_DIR, &MS_DIR[n], M_SZ);
                memcpy(&M_COORD, &MS_COORD[n], M_SZ);
                calculate_clip_matrix(this);
            }
            break;
        case 3:
            if (n != MS_TEXTURE_PTR) memcpy(&M_TEXTURE, &MS_TEXTURE[n & 1], M_SZ);
            break;
    }
}

static void cmd_MTX_IDENTITY(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0:
            identity_matrix(M_PROJECTION.m);
            printfcd("\nMTX_IDENTITY(projection);");
            //pprint_matrix("projection", M_TEXTURE.m);
            calculate_clip_matrix(this);
            return;
        case 1:
            identity_matrix(M_COORD.m);
            printfcd("\nMTX_IDENTITY(coord);");
            //pprint_matrix("coord", M_COORD.m);
            calculate_clip_matrix(this);
            return;
        case 2:
            identity_matrix(M_COORD.m);
            identity_matrix(M_DIR.m);
            printfcd("\nMTX_IDENTITY(coord&dir);");
            //pprint_matrix("coord", M_COORD.m);
            //pprint_matrix("dir", M_DIR.m);
            calculate_clip_matrix(this);
            return;
        case 3:
            identity_matrix(M_TEXTURE.m);
            printfcd("\nMTX_IDENTITY(texture);");
            //pprint_matrix("texture", M_TEXTURE.m);
            return;
    }
}

static void load_4x4(i32 *dest, i32 *src)
{
    for (u32 i = 0; i < 16; i++) {
        dest[i] = src[i];
    }
}

static void load_4x3(i32 *dest, i32 *src)
{
    for (u32 i = 0; i < 12; i++) {
        dest[i] = src[i];
    }
    dest[12] = dest[13] = dest[14] = 0;
    dest[15] = 1 << 12;
}

static void cmd_MTX_LOAD_4x4(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0:
            load_4x4(M_PROJECTION.m, (i32 *)DATA);
            printfcd("\nMTX_LOAD_4x4(PROJ);");
            calculate_clip_matrix(this);
            return;
        case 1:
            load_4x4(M_COORD.m, (i32 *)DATA);
            printfcd("\nMTX_LOAD_4x4(COORD);");
            calculate_clip_matrix(this);
            return;
        case 2:
            load_4x4(M_COORD.m, (i32 *)DATA);
            load_4x4(M_DIR.m, (i32 *)DATA);
            printfcd("\nMTX_LOAD_4x4(COORD&DIR);");
            calculate_clip_matrix(this);
            return;
        case 3:
            load_4x4(M_TEXTURE.m, (i32 *)DATA);
            printfcd("\nMTX_LOAD_4x4(TEXTURE);");
            return;
    }
}

static void cmd_MTX_LOAD_4x3(struct NDS *this)
{
    printfcd("\nMTX_LOAD_4x3");
    switch(this->ge.io.MTX_MODE) {
        case 0:
            load_4x3(M_PROJECTION.m, (i32 *)DATA);
            calculate_clip_matrix(this);
            return;
        case 1:
            load_4x3(M_COORD.m, (i32 *)DATA);
            calculate_clip_matrix(this);
            return;
        case 2:
            load_4x3(M_COORD.m, (i32 *)DATA);
            load_4x3(M_DIR.m, (i32 *)DATA);
            calculate_clip_matrix(this);
            return;
        case 3:
            load_4x3(M_TEXTURE.m, (i32 *)DATA);
            return;
    }
}

// B = A * B
static void matrix_multiply_4x4(i32 *A, i32 *B)
{
    i32 tmp_B[16];
    memcpy(tmp_B, B, sizeof(tmp_B));

    for (u32 x = 0; x < 4; x++) {
        for (u32 y = 0; y < 16; y+=4) {
            //cyx = ay1*b1x + ay2*b2x + ay3*b3x + ay4*b4x
            i64 out = ((i64)A[y+0] * (i64)tmp_B[x]) >> 12;
            out += ((i64)A[y+1] * (i64)tmp_B[4+x]) >> 12;
            out += ((i64)A[y+2] * (i64)tmp_B[8+x]) >> 12;
            out += ((i64)A[y+3] * (i64)tmp_B[12+x]) >> 12;
            B[x+y] = (i32)out;
        }
    }
}

static void matrix_multiply_4x3(i32 *A, i32 *B)
{
    // B = A(4x3) * B(4x4)
    i32 tmp_A[16];
    i32 tmp_B[16];
    memcpy(tmp_B, B, sizeof(tmp_B));
    memset(tmp_A, 0, sizeof(tmp_A));

    u32 i = 0;
    for (u32 y = 0; y < 16; y+=4) {
        for (u32 x = 0; x < 3; x++) {
            tmp_A[y+x] = A[i++];
        }
    }
    tmp_A[15] = 1 << 12;

    for (u32 x = 0; x < 4; x++) {
        for (u32 y = 0; y < 16; y+=4) {
            //cyx = ay1*b1x + ay2*b2x + ay3*b3x + ay4*b4x
            i64 out = ((i64)tmp_A[y+0] * (i64)tmp_B[x]) >> 12;
            out += ((i64)tmp_A[y+1] * (i64)tmp_B[4+x]) >> 12;
            out += ((i64)tmp_A[y+2] * (i64)tmp_B[8+x]) >> 12;
            out += ((i64)tmp_A[y+3] * (i64)tmp_B[12+x]) >> 12;
            B[x+y] = (i32)out;
        }
    }
}

static void matrix_multiply_3x3(i32 *A, i32 *B)
{
    // B(4x4) = A(3x3) * B(4x4)
    i32 tmp_A[16];
    i32 tmp_B[16];
    memcpy(tmp_B, B, sizeof(tmp_B));

    u32 i =0;
    for (u32 y = 0; y < 12; y+=4) {
        for (u32 x = 0; x < 3; x++) {
            tmp_A[y + x] = A[i++];
        }
    }
    tmp_A[3] = tmp_A[7] = tmp_A[11] = 0;
    tmp_A[12] = tmp_A[13] = tmp_A[14] = 0;
    tmp_A[15] = 1 << 12;


    for (u32 x = 0; x < 4; x++) {
        for (u32 y = 0; y < 16; y+=4) {
            //cyx = ay1*b1x + ay2*b2x + ay3*b3x + ay4*b4x
            i64 out = ((i64)tmp_A[y+0] * (i64)tmp_B[x]) >> 12;
            out += ((i64)tmp_A[y+1] * (i64)tmp_B[4+x]) >> 12;
            out += ((i64)tmp_A[y+2] * (i64)tmp_B[8+x]) >> 12;
            out += ((i64)tmp_A[y+3] * (i64)tmp_B[12+x]) >> 12;
            B[x+y] = (i32)out;
        }
    }

    //printfcd("\nOut matrix:");
    //pprint_matrix(B);
}


static void cmd_MTX_MULT_4x4(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0: // projection
            printfcd("\nMTX_MULT_4x4(proj);");
            //pprint_matrix("projection before", M_PROJECTION.m);
            //pprint_matrix("input matrix", (i32 *)DATA);
            matrix_multiply_4x4((i32 *)DATA, M_PROJECTION.m);
            //pprint_matrix("projection result", M_PROJECTION.m);
            calculate_clip_matrix(this);
            return;
        case 1: //  pos/coord matrix
            printfcd("\nMTX_MULT_4x4(coord);");
            matrix_multiply_4x4((i32 *)DATA, M_COORD.m);
            calculate_clip_matrix(this);
            return;
        case 2: // both pos/coord and dir/vector matrices
            printfcd("\nMTX_MULT_4x4(coord&dir);");
            matrix_multiply_4x4((i32 *)DATA, M_COORD.m);
            matrix_multiply_4x4((i32 *)DATA, M_DIR.m);
            calculate_clip_matrix(this);
            return;
        case 3: // texture matrix
            printfcd("\nMTX_MULT_4x4(texture);");
            matrix_multiply_4x4((i32 *)DATA, M_TEXTURE.m);
            return;
    }
}

static void matrix_scale(i32 *B, i32 *A)
{
    i32 tmp_A[16];
    i32 tmp_B[16];
    memcpy(tmp_B, B, sizeof(tmp_B));
    memset(tmp_A, 0, sizeof(tmp_A));
    tmp_A[0] = A[0];
    tmp_A[5] = A[1];
    tmp_A[10] = A[2];
    tmp_A[15] = 1 << 12;

    for (u32 x = 0; x < 4; x++) {
        for (u32 y = 0; y < 16; y+=4) {
            //cyx = ay1*b1x + ay2*b2x + ay3*b3x + ay4*b4x
            i64 out = ((i64)tmp_A[y+0] * (i64)tmp_B[x]) >> 12;
            out += ((i64)tmp_A[y+1] * (i64)tmp_B[4+x]) >> 12;
            out += ((i64)tmp_A[y+2] * (i64)tmp_B[8+x]) >> 12;
            out += ((i64)tmp_A[y+3] * (i64)tmp_B[12+x]) >> 12;
            B[x+y] = (i32)out;
        }
    }
}

static void matrix_translate(i32 *B, i32 *A)
{
    i32 tmp_A[16];
    i32 tmp_B[16];
    memcpy(tmp_B, B, sizeof(tmp_B));
    memset(tmp_A, 0, sizeof(i32) * 12);
    tmp_A[0] = tmp_A[5] = tmp_A[10] = tmp_A[15] = 1 << 12;
    tmp_A[12] = A[0];
    tmp_A[13] = A[1];
    tmp_A[14] = A[2];
    //printfcd("\n\n---Matrix translate. Tranlsation matrix:");
    //pprint_matrix(tmp_A);

    //printfcd("\n\nMatrix to multiply by:");
    //pprint_matrix(tmp_B);

    for (u32 x = 0; x < 4; x++) {
        for (u32 y = 0; y < 16; y+=4) {
            //cyx = ay1*b1x + ay2*b2x + ay3*b3x + ay4*b4x
            i64 out = ((i64)tmp_A[y+0] * (i64)tmp_B[x]) >> 12;
            out += ((i64)tmp_A[y+1] * (i64)tmp_B[4+x]) >> 12;
            out += ((i64)tmp_A[y+2] * (i64)tmp_B[8+x]) >> 12;
            out += ((i64)tmp_A[y+3] * (i64)tmp_B[12+x]) >> 12;
            B[x+y] = (i32)out;
        }
    }
    //printfcd("\nResult matrix:");
    //pprint_matrix(B);
}


static void cmd_MTX_MULT_4x3(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0: // projection
            matrix_multiply_4x3((i32 *)DATA, M_PROJECTION.m);
            printfcd("\nMTX_MULT_4x3(Projection);");
            calculate_clip_matrix(this);
            return;
        case 1: //  pos/coord matrix
            matrix_multiply_4x3((i32 *)DATA, M_COORD.m);
            printfcd("\nMTX_MULT_4x3(Coord);");
            calculate_clip_matrix(this);
            return;
        case 2: // both pos/coord and dir/vector matrices
            printfcd("\nMTX_MULT_4x3(coord&dir);");
            pprint_matrix("coord before", M_COORD.m);
            matrix_multiply_4x3((i32 *)DATA, M_COORD.m);
            matrix_multiply_4x3((i32 *)DATA, M_DIR.m);
            pprint_matrix("coord after", M_COORD.m);
            calculate_clip_matrix(this);
            return;
        case 3: // texture matrix
            matrix_multiply_4x3((i32 *)DATA, M_TEXTURE.m);
            printfcd("\nMTX_MULT_4x3(texture);");
            return;
    }
}

static void cmd_MTX_MULT_3x3(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0: // projection
            matrix_multiply_3x3((i32 *)DATA, M_PROJECTION.m);
            printfcd("\nMULT_3x3 Projection");
            calculate_clip_matrix(this);
            return;
        case 1: //  pos/coord matrix
            matrix_multiply_3x3((i32 *)DATA, M_COORD.m);
            printfcd("\nMULT_3x3 Pos/Coord");
            calculate_clip_matrix(this);
            return;
        case 2: // both pos/coord and dir/vector matrices
            printfcd("\nMULT_3x3 Pos/Coord and dir/vector");
            matrix_multiply_3x3((i32 *)DATA, M_COORD.m);
            matrix_multiply_3x3((i32 *)DATA, M_DIR.m);
            calculate_clip_matrix(this);
            return;
        case 3: // texture matrix
            printfcd("\nMULT_3x3 Texture and dir/vector");
            matrix_multiply_3x3((i32 *)DATA, M_TEXTURE.m);
            return;
    }
}

static void cmd_MTX_SCALE(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0:
            matrix_scale(M_PROJECTION.m, (i32 *)DATA);
            printfcd("\nMTX_SCALE proj");
            calculate_clip_matrix(this);
            return;
        case 1:
        case 2:
            matrix_scale(M_COORD.m, (i32 *)DATA);
            printfcd("\nMTX_SCALE COORD proj");
            calculate_clip_matrix(this);
            return;
        case 3:
            printfcd("\nMTX_SCALE texture");
            matrix_scale(M_TEXTURE.m, (i32 *)DATA);
            return;
    }
}

static void cmd_MTX_TRANS(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0:
            matrix_translate(M_PROJECTION.m, (i32 *)DATA);
            printfcd("\nMTX_TRANS projection");
            calculate_clip_matrix(this);
            return;
        case 1:
            matrix_translate(M_COORD.m, (i32 *)DATA);
            printfcd("\nMTX_TRANS coordinate");
            calculate_clip_matrix(this);
            return;
        case 2:
            matrix_translate(M_COORD.m, (i32 *)DATA);
            matrix_translate(M_DIR.m, (i32 *)DATA);
            printfcd("\nMTX_TRANS coord/dir");
            calculate_clip_matrix(this);
            return;
        case 3:
            printfcd("\nMTX_TRANS texture");
            matrix_translate(M_TEXTURE.m, (i32 *)DATA);
            return;
    }
}

static void cmd_COLOR(struct NDS *this)
{
    u32 color_in = DATA[0];
    u32 r = (color_in >> 0) & 31;
    u32 g = (color_in >> 5) & 31;
    u32 b = (color_in >> 10) & 31;
    if (r) r = (r << 1) + 1;
    if (g) g = (g << 1) + 1;
    if (b) b = (b << 1) + 1;
    printfcd("\nCOLOR r:%d g:%d b:%d", r, g, b);
    this->ge.params.vtx.color = r | (g << 6) | (b << 12);
}

static void cmd_NORMAL(struct NDS *this)
{
    static int a = 1;
    if (a) {
        printf("\nIMPLEMENT NORMAL CMD! IGNORED");
        a = 0;
        return;
    }
    /*

  IF TexCoordTransformMode=2 THEN TexCoord=NormalVector*Matrix (see TexCoord)
  NormalVector=NormalVector*DirectionalMatrix
  VertexColor = EmissionColor
  FOR i=0 to 3
   IF PolygonAttrLight[i]=enabled THEN
    DiffuseLevel = max(0,-(LightVector[i]*NormalVector))
    ShininessLevel = max(0,(-HalfVector[i])*(NormalVector))^2
    IF TableEnabled THEN ShininessLevel = ShininessTable[ShininessLevel]
    ;note: below processed separately for the R,G,B color components...
    VertexColor = VertexColor + SpecularColor*LightColor[i]*ShininessLevel
    VertexColor = VertexColor + DiffuseColor*LightColor[i]*DiffuseLevel
    VertexColor = VertexColor + AmbientColor*LightColor[i]
   ENDIF
  NEXT i     */
}

static void cmd_TEXCOORD(struct NDS *this)
{
    printfcd("\nTEXCOORD");
    // 1bit sign, 11bit integer, 4bit fraction
    this->ge.params.vtx.S = (i32)(i16)(DATA[0] & 0xFFFF);
    this->ge.params.vtx.T = (i32)(i16)(DATA[0] >> 16);
}

#define CCW 0
#define CW 1

static void matrix_multiply_by_vector(i32 *dest, i32 *matrix, i32 *src)
{
    //printfcd("\nMultiply matrix by vector.");
    //printfcd("\nVector: %f %f %f %f", vtx_to_float(src[0]), vtx_to_float(src[1]), vtx_to_float(src[2]), vtx_to_float(src[3]));
    //printfcd("\nmatrix: ");
    //pprint_matrix(matrix);
    // c[x] = a[x]*b1x + a[y]*b2x + a[y]*b3x + a[y]*b4x
    for (u32 x = 0; x < 4; x++) {
        i64 out = ((i64)src[0]*(i64)matrix[0+x])>>12;
        out += ((i64)src[1]*(i64)matrix[4+x])>>12;
        out += ((i64)src[2]*(i64)matrix[8+x])>>12;
        out += ((i64)src[3]*(i64)matrix[12+x])>>12;
        dest[x] = (i32)out;
    }
    //printfcd("\nVector after: %f %f %f %f", vtx_to_float(dest[0]), vtx_to_float(dest[1]), vtx_to_float(dest[2]), vtx_to_float(dest[3]));
}

static void transform_vertex_on_ingestion(struct NDS *this, struct NDS_GE_VTX_node *node)
{
    node->color = this->ge.params.vtx.color;
    node->uv[0] = this->ge.params.vtx.S;
    node->uv[1] = this->ge.params.vtx.T;
    node->processed = 0;
    //printfcd("\nVtx in: %f %f %f", vtx_to_float(src->xyzw[0]), vtx_to_float(src->xyzw[1]), vtx_to_float(src->xyzw[2]));

    // TODO: add any I missed first pass
    if (this->ge.params.poly.current.tex_param.texture_coord_transform_mode == NDS_TCTM_vertex) {
        for (u32 i = 0; i < 2; i++) {
            i64 x = node->xyzw[0] * M_TEXTURE.m[i*4]; // TODO: May have got x/y wrong here
            i64 y = node->xyzw[1] * M_TEXTURE.m[(i*4)+1];
            i64 z = node->xyzw[2] * M_TEXTURE.m[(i*4)+2];

            node->uv[i] = (i16)(((x + y + z) >> 24) + node->uv[i]);
        }
    }
    i32 tmp[4];
    memcpy(tmp, node->xyzw, sizeof(i32)*4);
    matrix_multiply_by_vector(node->xyzw, M_CLIP.m, tmp);
    //printfcd("\nTransformed vertex: %f %f %f %f", vtx_to_float(dest->xyzw[0]), vtx_to_float(dest->xyzw[1]), vtx_to_float(dest->xyzw[2]), vtx_to_float(dest->xyzw[3]));
}

static void clip_verts(struct NDS *this)
{
    // TODO: this
}

static u32 determine_needs_clipping(struct NDS_GE_VTX_node *v)
{
    if (v->processed) return 0;
    if (v->num_children > 0) {
        for (u32 i = 0; i < v->num_children; i++) {
            if (determine_needs_clipping(v->children[i])) return 1;
        }
        return 0;
    }

    i32 w = v->xyzw[3];

    for (int i = 0; i < 3; i++) {
        if (v->xyzw[i] < -w || v->xyzw[i] > w) {
            return 1;
        }
    }
    return 0;
}


static struct NDS_GE_VTX_node *find_first_leaf(struct NDS_GE_VTX_node *node)
{
    // Go down children[0] until there are none!
    while(node->num_children > 0) {
        node = node->children[0];
    }
    return node;
}

static struct NDS_GE_VTX_node *next_leaf(struct NDS_GE_VTX_node *node)
{
    // When given a leaf...
    // Seek up.
    // Determine our posiiton in list
    // Recurse down next child if available using find_first_leaf,
    // Else go up to parent.
    // If no parent. return NULL
    while (node->parent != NULL) {
        struct NDS_GE_VTX_node *child = node;
        node = node->parent;
        // Determine if we have any more children left...
        for (u32 i = 0; i < node->num_children; i++) {
            if (node->children[i] == child) { // We want to seek the next...
                u32 seek_down = i + 1;
                if (seek_down >= node->num_children) break; // We were the last child!

                // We were not the last child. So return the next leaf...
                return find_first_leaf(node->children[seek_down]);
            }
        }
    }

    return NULL; // We were given the last leaf!
}

static u32 commit_vertex(struct NDS *this, i32 xx, i32 yy, i32 zz, i32 ww, i32 *uv, u32 color)
{
    struct NDS_GE_BUFFERS *b = &this->ge.buffers[this->ge.ge_has_buffer];
    u32 addr = b->vertex_index;

    b->vertex_index++;
    if (b->vertex_index >= 6144) {
        this->re.io.DISP3DCNT.poly_vtx_ram_overflow = 1;
    }

    struct NDS_RE_VERTEX *v = &b->vertex[addr];

    // We need...xx, yy, zz, ww, uv, and color
    // 32 bits each = 24 bytes...
    v->xx = xx;
    v->yy = yy;
    v->zz = zz;
    v->ww = ww;
    v->uv[0] = uv[0];
    v->uv[1] = uv[1];
    v->color = color;

    return addr;
}

static u32 finalize_verts_and_get_first_addr(struct NDS *this)
{
    // Final transform to screen and write into vertex buffer

    // Go thru verts...
    struct NDS_GE_VTX_node *first_node = find_first_leaf(&VTX_ROOT);
    struct NDS_GE_VTX_node *node = first_node;
    while(node != NULL) {
        if (!node->processed) {
            node->processed = 1;

            i32 x = node->xyzw[0];
            i32 y = node->xyzw[1];
            i32 z = node->xyzw[2];
            i32 w = node->xyzw[3];

            w &= 0xFFFFFF;
            i32 scrX, scrY;
            if (w > 0) {
                scrX = x + w;
                scrY = -y + w;
                i32 den = w;

                // According to melonDS, the NDS uses a 32-bit divider.
                // If the W coordinate is larger than 16 bits, the hardware seems to sacrifice some precision to fit the numbers
                // in the divider.
                if (w > 0xFFFF) {
                    scrX >>= 1;
                    scrY >>= 1;
                    den >>= 1;
                }

                den <<= 1;
                scrX = ((scrX * this->re.io.viewport.width) / den) + this->re.io.viewport.x0;
                scrY = ((scrY * this->re.io.viewport.height) / den) + this->re.io.viewport.y0;
                scrX &= 0x1FF;
                scrY &= 0xFF;
            } else {
                scrX = 0;
                scrY = 0;
            }
            node->vram_ptr = commit_vertex(this, scrX, scrY, node->xyzw[2], node->xyzw[3], node->uv, node->color);
        }
        node = next_leaf(node);
    }
    return first_node->vram_ptr;
}

static u32 count_leafs(struct NDS_GE_VTX_node *node, u32 total_so_far)
{
    if (node->num_children == 0) return total_so_far+1;

    for (u32 i = 0; i < node->num_children; i++) {
        total_so_far = count_leafs(node->children[i], total_so_far);
    }
    return total_so_far;
}

static u32 set_bitmask_for_edge(struct NDS_RE_POLY *poly, struct NDS_RE_VERTEX *v[])
{
    u32 y1, y2;
    if (v[0]->yy > v[1]->yy) {
        y1 = v[1]->yy;
        y2 = v[0]->yy;
    }
    else {
        y1 = v[0]->yy;
        y2 = v[1]->yy;
    }
    for (u32 y = y1; y < y2; y++) {
        if (y > 192) {
            printf("\nDONE EFFED UP!");
            break;
        }
        u32 bytenum = y >> 3;
        u32 bitnum = y & 7;
        poly->lines_on_bitfield[bytenum] |= 1 << bitnum;
    }
    return v[0]->yy < v[1]->yy;
}

static u32 determine_highest_vertex(struct NDS_RE_VERTEX *vertices, u32 num_vertices)
{
    u32 highest = 0;
    for (u32 i = 1; i < num_vertices; i++) {
        if (vertices[i].yy > vertices[highest].yy) highest = i;
    }
    return highest;
}

static u32 determine_winding_order(struct NDS_RE_VERTEX *vertices, u32 num_vertices, u32 highest)
{
    // OK, now progress in order until we have a y-delta of >0.
    i32 cx = vertices[highest].xx;
    for (u32 i = 1; i < num_vertices; i++) {
        u32 index_to_compare = (i + highest) % num_vertices;
        struct NDS_RE_VERTEX *cmp = &vertices[index_to_compare];
        i32 diff = (i32)cmp->xx - cx;
        if (diff == 0) continue;
        if (diff > 0) { // Positive X-slope from least y-point means CW
            return CW;
        }
        else return CCW;
    }
    // If we got here, we are a vertical line and it doesn't really matter?
    return CCW;
}

static void evaluate_edges(struct NDS *this, struct NDS_RE_POLY *poly, u32 expected_winding_order)
{
    struct NDS_GE_BUFFERS *b = &this->ge.buffers[this->ge.ge_has_buffer];
    struct NDS_RE_VERTEX *v[2];
    v[1] = &b->vertex[poly->first_vertex_ptr];
    memset(poly->lines_on_bitfield, 0, sizeof(poly->lines_on_bitfield));
    u32 edgenum = 0;

    poly->highest_vertex = determine_highest_vertex(v[1], poly->num_vertices);
    u32 winding_order = determine_winding_order(v[1], poly->num_vertices, poly->highest_vertex);
    poly->front_facing = winding_order == expected_winding_order;
    poly->winding_order = winding_order;
    poly->edge_r_bitfield = 0;
    poly->front_facing = 0;
    printf("\nPOLY WINDING ORDER: %d", poly->winding_order);

    for (u32 i = 1; i <= poly->num_vertices; i++) {
        v[0] = v[1];
        v[1] = &b->vertex[(poly->first_vertex_ptr + i) % poly->num_vertices];
        u32 top_to_bottom = set_bitmask_for_edge(poly, v) ^ 1;

        printf("\nV0 %d,%d V1 %d,%d top_to_bottom:%d", v[0]->xx, v[0]->yy, v[1]->xx, v[1]->yy, top_to_bottom);

        if (poly->winding_order == CCW) {
            //printf("\nTOP TO BOTTOM? %d")
            // top to bottom means left edge on CCW
            // but remember Y is reversed...
            if (top_to_bottom) poly->edge_r_bitfield |= (1 << edgenum);
        }
        else {
            // top to bottom means right edge on CW
            if (!top_to_bottom) poly->edge_r_bitfield |= (1 << edgenum);
        }
        edgenum++;
    }

    v[0] = v[1];
    v[1] = &b->vertex[poly->first_vertex_ptr];
    u32 top_to_bottom = set_bitmask_for_edge(poly, v);
    if (poly->winding_order == CCW) {
        // top to bottom means left edge on CCW
        if (!top_to_bottom) poly->edge_r_bitfield |= (1 << edgenum);
    }
    else {
        // top to bottom means right edge on CW
        if (top_to_bottom) poly->edge_r_bitfield |= (1 << edgenum);
    }
}

static void ingest_poly(struct NDS *this, u32 winding_order) {
    struct NDS_GE_BUFFERS *b = &this->ge.buffers[this->ge.ge_has_buffer];
    u32 addr = b->polygon_index;

    b->polygon_index++;
    if (b->polygon_index >= 2048) {
        this->re.io.DISP3DCNT.poly_vtx_ram_overflow = 1;
    }

    struct NDS_RE_POLY *out = &b->polygon[addr];
    out->attr.u = this->ge.params.poly.current.attr.u;
    out->tex_param.u = this->ge.params.poly.current.tex_param.u;
    // TODO: add all relavent attributes to structure here

    // Now clip...
    u32 needs_clipping = 0;
    for (u32 vn = 0; vn < VTX_ROOT.num_children; vn++) {
        struct NDS_GE_VTX_node *v = VTX_ROOT.children[vn];
        needs_clipping = determine_needs_clipping(v);
        if (needs_clipping) break;
    }

    // Clip vertices!
    if (needs_clipping) {
        clip_verts(this);
    }

    // GO through leafs.
    // For any unprocessed vertices, add to polygorm RAM.
    out->first_vertex_ptr = finalize_verts_and_get_first_addr(this);
    if (this->re.io.DISP3DCNT.poly_vtx_ram_overflow) {
        b->polygon_index--;
        return;
    }

    out->num_vertices = count_leafs(&VTX_ROOT, 0);

    evaluate_edges(this, out, winding_order);

    printfcd("\nOUTPUT POLY HAS %d SIDES", out->num_vertices);
}

static struct NDS_GE_VTX_node *node_list_alloc(struct NDS *this)
{
    assert(VTX_ALIST.list_len > 0);
    struct NDS_GE_VTX_node *o = VTX_ALIST.items[VTX_ALIST.list_len];
    VTX_ALIST.list_len--;
    return o;
}

static void node_list_free(struct NDS *this, struct NDS_GE_VTX_node *node)
{
    // Put vertex back onto list
    node->num_children = 0;
    node->processed = 0;
    VTX_ALIST.list_len++;
    assert(VTX_ALIST.list_len < NDS_GE_VTX_LIST_MAX);
    VTX_ALIST.items[VTX_ALIST.list_len] = node;
}

static void delete_node_children(struct NDS *this, struct NDS_GE_VTX_node *node);

static void delete_node_children(struct NDS *this, struct NDS_GE_VTX_node *node)
{
    for (u32 i = 0; i < node->num_children; i++) {
        struct NDS_GE_VTX_node *n = node->children[i];
        delete_node_children(this, n);
    }
    node_list_free(this, node);
}

static void delete_vtx_cache(struct NDS *this)
{
    for (u32 i = 0; i < VTX_ROOT.num_children; i++) {
        struct NDS_GE_VTX_node *n = VTX_ROOT.children[i];
        delete_node_children(this, n);
    }
    VTX_ROOT.num_children = 0;
}

static void pop1_vtx_cache(struct NDS *this)
{
    // Just pop 1 off of it...
    assert(VTX_ROOT.num_children>0);
    delete_node_children(this, VTX_ROOT.children[0]);
    VTX_ROOT.children[0] = VTX_ROOT.children[1];
    VTX_ROOT.children[1] = VTX_ROOT.children[2];
    VTX_ROOT.children[2] = VTX_ROOT.children[3];
    VTX_ROOT.children[3] = NULL;
    VTX_ROOT.num_children--;
}

static struct NDS_GE_VTX_node *vertex_add_child(struct NDS *this, struct NDS_GE_VTX_node *node)
{
    struct NDS_GE_VTX_node *o = node_list_alloc(this);
    o->parent = node;
    node->children[node->num_children++] = o;
    assert(node->num_children < 4);
    return o;
}

static void ingest_vertex(struct NDS *this, i32 x, i32 y, i32 z) {
    if (this->re.io.DISP3DCNT.poly_vtx_ram_overflow) {
        printf("\nVTX OVERFLOW...");
        return;
    }
    // Add to vertex cache
    struct NDS_GE_VTX_node *o = vertex_add_child(this, &VTX_ROOT);
    o->xyzw[0] = x;
    o->xyzw[1] = y;
    o->xyzw[2] = z;
    o->xyzw[3] = 1 << 12;
    o->processed = 0;
    o->uv[0] = this->ge.params.vtx.S;
    o->uv[1] = this->ge.params.vtx.T;
    transform_vertex_on_ingestion(this, o);

    switch(this->ge.params.vtx_strip.mode) {
        case NDS_GEM_SEPERATE_TRIANGLES:
            if (VTX_ROOT.num_children >= 3) {
                printfcd("\nDO POLY SEPARATE TRI");
                ingest_poly(this, CCW);
                // clear the cache
                delete_vtx_cache(this);
            }
            break;
        case NDS_GEM_SEPERATE_QUADS:
            if (VTX_ROOT.num_children >= 4) {
                printfcd("\nDO POLY SEPARATE QUAD");
                ingest_poly(this, CCW);
                delete_vtx_cache(this);
            }
            break;
        case NDS_GEM_TRIANGLE_STRIP:
            if (VTX_ROOT.num_children >= 3) {
                ingest_poly(this, this->ge.winding_order);
                this->ge.winding_order ^= 1;
                pop1_vtx_cache(this);
            }
            break;
        case NDS_GEM_QUAD_STRIP:
            if (VTX_ROOT.num_children >= 4) {
                ingest_poly(this, this->ge.winding_order);
                this->ge.winding_order ^= 1;
                pop1_vtx_cache(this);
                pop1_vtx_cache(this);
            }
            break;
    }
}

static void cmd_VTX_16(struct NDS *this)
{
    // unpack vertex and send it to our vertex stream
    //0-15 is X
    //16-31 is Y
    //next parm 0-15 Z
    printfcd("\nVTX_16: %f %f %f", vtx_to_float((i32)(i16)(DATA[0] & 0xFFFF)), vtx_to_float((i32)(i16)(DATA[0] >> 16)), vtx_to_float((i32)(i16)(DATA[1] & 0xFFFF)));
    ingest_vertex(this, (i32)(i16)(DATA[0] & 0xFFFF), (i32)(i16)(DATA[0] >> 16), (i32)(i16)(DATA[1] & 0xFFFF));
    //i32 x = DATA[0] &
}
//static void cmd_MTX_POP(struct NDS *this)

static void cmd_SWAP_BUFFERS(struct NDS *this)
{
    printfcd("\n\n---------------------------\nSWAP_BUFFERS!");
    this->ge.io.swap_buffers = 1;
    this->ge.buffers[this->ge.ge_has_buffer].translucent_y_sorting_manual = DATA[0] & 1;
    this->ge.buffers[this->ge.ge_has_buffer].depth_buffering_w = (DATA[0] >> 1) & 1;
}

static void cmd_POLYGON_ATTR(struct NDS *this)
{
    printfcd("\nPOLYGON_ATTR");
    this->ge.params.poly.on_vtx_start.attr.u = DATA[0];
}

static void terminate_poly_strip(struct NDS *this)
{
    for (u32 i = 0; i < VTX_ROOT.num_children; i++) {
        delete_node_children(this, VTX_ROOT.children[i]);
    }
    VTX_ROOT.num_children = 0;
    this->ge.winding_order = 0;
}

static void cmd_VIEWPORT(struct NDS *this)
{
    u32 x0 = DATA[0] & 0xFF;
    u32 y0 = (DATA[0] >> 8) & 0xFF;
    u32 x1 = (DATA[0] >> 16) & 0xFF;
    u32 y1 = (DATA[0] >> 24) & 0xFF;

    printfcd("\nVIEWPORT(%d, %d, %d, %d)", x0, y0, x1, y1);
    this->re.io.viewport.x0 = x0;
    this->re.io.viewport.y0 = y0;
    this->re.io.viewport.x1 = x1;
    this->re.io.viewport.y1 = y1;
    this->re.io.viewport.width = 1 + x1 - x0;
    this->re.io.viewport.height = 1 +y1 - y0;
}

static void cmd_TEXIMAGE_PARAM(struct NDS *this)
{
    printfcd("\nTEXIMAGE_PARAM");
    this->ge.params.poly.current.tex_param.u = DATA[0];
}

static void cmd_BEGIN_VTXS(struct NDS *this)
{
    printfcd("\nBEGIN_VTXS");
    // Empty vertex cache, terminate strip...
    terminate_poly_strip(this);

    // Latch parameters. TODO: did I do them all?
    this->ge.params.poly.current.attr.u = this->ge.params.poly.on_vtx_start.attr.u;
}
#undef DATA

static void do_cmd(void *ptr, u64 cmd, u64 current_clock, u32 jitter)
{
    struct NDS *this = (struct NDS *)ptr;
    GXSTAT.ge_busy = 0;

    // Handle command!
    printfcd("\n");
    switch(cmd) {
#define dcmd(label) case NDS_GE_CMD_##label: cmd_##label(this); break
        dcmd(MTX_MODE);
        dcmd(MTX_PUSH);
        dcmd(MTX_POP);
        dcmd(MTX_STORE);
        dcmd(MTX_RESTORE);
        dcmd(MTX_IDENTITY);
        dcmd(MTX_LOAD_4x4);
        dcmd(MTX_LOAD_4x3);
        dcmd(MTX_MULT_4x4);
        dcmd(MTX_MULT_4x3);
        dcmd(MTX_MULT_3x3);
        dcmd(MTX_SCALE);
        dcmd(MTX_TRANS);
        dcmd(COLOR);
        dcmd(NORMAL);
        dcmd(TEXCOORD);
        dcmd(VTX_16);
        dcmd(SWAP_BUFFERS);
        dcmd(POLYGON_ATTR);
        dcmd(BEGIN_VTXS);
        dcmd(TEXIMAGE_PARAM);
        dcmd(VIEWPORT);
#undef dcmd
        default:
            printf("\nUnhandled GE cmd %02llx", cmd);
            break;
    }

    // Schedule next command if applicable!
    ge_handle_cmd(this);
}

static void ge_handle_cmd(struct NDS *this)
{
    // Pop a command!
    // Schedule it to be completed in N cycles!
    if (this->ge.io.swap_buffers) {
        //printfcd("\nSWAP BUFFERS PUSHEd, IGNORING FURTHER...");
        //return; // GE is locked up!
    }

    u32 cmd = 0;
    // Drain the FIFO looking for a good command!
    while(cmd == 0) {
        u32 a = fifo_pop_full_cmd(this, &cmd, this->ge.cur_cmd.data);
        if (!a) {
            return;
        }

        // END_VTXs is NOOP
        if (cmd == NDS_GE_CMD_END_VTXS) cmd = 0;

        if (cmd != 0) {
            assert(tbl_cmd_good[cmd]);
        }
    }

    GXSTAT.ge_busy = 1;
    i32 num_cycles = get_num_cycles(this, cmd);
    if (cmd == NDS_GE_CMD_SWAP_BUFFERS) {
        num_cycles = 1;
    }

    scheduler_only_add_abs(&this->scheduler, NDS_clock_current7(this) + num_cycles, cmd, this, &do_cmd, NULL);
}

void NDS_GE_FIFO_write2(struct NDS *this, u8 cmd, u32 val)
{
    struct NDS_GE *ge = &this->ge;
    if (ge->io.fifo_in.pos == 0) {
        NDS_GE_FIFO_write(this, cmd);
    }
    NDS_GE_FIFO_write(this, val);
}

void NDS_GE_FIFO_write(struct NDS *this, u32 val)
{
    struct NDS_GE *ge = &this->ge;
    u32 pos = ge->io.fifo_in.pos++;
    ge->io.fifo_in.buf[pos] = val;
    assert(pos<160);
    if (pos == 0) { // Parse command!
        ge->io.fifo_in.num_cmds = 0;
        for (u32 i = 0; i < 4; i++) {
            ge->io.fifo_in.cmds[i] = val & 0xFF;
            if (val & 0xFF) ge->io.fifo_in.num_cmds = i+1;
            val >>= 8;
        }
        if (ge->io.fifo_in.num_cmds == 0) { // empty command...
            return;
        }

        u32 total_params = 0;
        u32 fifo_pos = 1;
        for (u32 i = 0; i < ge->io.fifo_in.num_cmds; i++) {
            u32 cmd = ge->io.fifo_in.cmds[i];
            if (tbl_cmd_good[cmd]) {
                ge->io.fifo_in.cmd_num_params[i] = tbl_num_params[cmd];
                if ((i == 0) && (ge->io.fifo_in.cmd_num_params[i] == 0)) ge->io.fifo_in.cmd_num_params[i] = 1;
            }
            total_params += ge->io.fifo_in.cmd_num_params[i];
            ge->io.fifo_in.cmd_pos[i] = fifo_pos;
            fifo_pos += total_params;
        }
        ge->io.fifo_in.total_len = total_params + 1;
    }
    if (ge->io.fifo_in.pos >= ge->io.fifo_in.total_len) {
        // Commands finish. Now add them to the FIFO...
        for (u32 i = 0; i < ge->io.fifo_in.num_cmds; i++) {
            u32 first_param = ge->io.fifo_in.buf[ge->io.fifo_in.cmd_pos[i]];
            fifo_push(this, ge->io.fifo_in.cmds[i], first_param);
            if (ge->io.fifo_in.cmd_num_params[i] > 1) {
                for (u32 j = 1; j < ge->io.fifo_in.cmd_num_params[i]; j++) {
                    fifo_push(this, 0, ge->io.fifo_in.buf[ge->io.fifo_in.cmd_pos[i] + j]);
                }
            }
            ge->io.fifo_in.pos = 0;
        }
    }
    if (!GXSTAT.ge_busy) ge_handle_cmd(this);
}

u32 NDS_GE_check_irq(struct NDS *this)
{
    switch(GXSTAT.cmd_fifo_irq_mode)
    {
        case 0:
            return 0;
        case 1:
            return GXSTAT.cmd_fifo_less_than_half_full;
        case 2:
            return GXSTAT.cmd_fifo_empty;
        case 3:
            printf("\nWARN BAD IRQ MODE FOR GE FIFO");
            return 0;
    }
    return 0;
}

void NDS_GE_write(struct NDS *this, u32 addr, u32 sz, u32 val)
{
    switch(addr) {
        case R9_CLEAR_COLOR:
            this->re.io.clear.color = val & 0x7FFF;
            this->re.io.clear.fog_to_rear_plane = (val >> 15) & 1;
            this->re.io.clear.alpha = (val >> 16) & 0x1F;
            this->re.io.clear.poly_id = (val >> 24) & 0x3F;
            return;
        case R9_CLEAR_DEPTH:
            assert(sz==2);
            val &= 0x7FFF;
            // The 15bit Depth is expanded to 24bit as "X=(X*200h)+((X+1)/8000h)*1FFh".
            this->re.io.clear.z = (val * 0x200) + ((val + 1) / 0x8000) * 0x1FF;
            return;
        case R9_GXFIFO:
            NDS_GE_FIFO_write(this, val);
            return;
        case R9_GXSTAT:
            assert(sz==4);
            if (val & 0x8000) {
                GXSTAT.matrix_stack_over_or_underflow_error = 0;
                GXSTAT.projection_matrix_stack_level = 0;
                this->ge.matrices.stacks.coordinate_direction_ptr = 0;
                this->ge.matrices.stacks.texture_ptr = 0;
            }
            GXSTAT.cmd_fifo_irq_mode = (val >> 30) & 3;
            if (NDS_GE_check_irq(this))
                NDS_update_IF9(this, NDS_IRQ_GXFIFO);
            return;


#define gcmd(label) case R9_G_CMD_##label: NDS_GE_FIFO_write2(this, NDS_GE_CMD_##label, val); return
        gcmd(MTX_MODE);
        gcmd(MTX_PUSH);
        gcmd(MTX_POP);
        gcmd(MTX_STORE);
        gcmd(MTX_RESTORE);
        gcmd(MTX_IDENTITY);
        gcmd(MTX_LOAD_4x4);
        gcmd(MTX_LOAD_4x3);
        gcmd(MTX_MULT_4x4);
        gcmd(MTX_MULT_4x3);
        gcmd(MTX_MULT_3x3);
        gcmd(MTX_SCALE);
        gcmd(MTX_TRANS);
        gcmd(COLOR);
        gcmd(NORMAL);
        gcmd(TEXCOORD);
        gcmd(VTX_16);
        gcmd(VTX_10);
        gcmd(VTX_XY);
        gcmd(VTX_XZ);
        gcmd(VTX_YZ);
        gcmd(VTX_DIFF);
        gcmd(POLYGON_ATTR);
        gcmd(TEXIMAGE_PARAM);
        gcmd(PLTT_BASE);
        gcmd(DIFF_AMB);
        gcmd(SPE_EMI);
        gcmd(LIGHT_VECTOR);
        gcmd(LIGHT_COLOR);
        gcmd(SHININESS);
        gcmd(BEGIN_VTXS);
        gcmd(END_VTXS);
        gcmd(SWAP_BUFFERS);
        gcmd(VIEWPORT);
        gcmd(BOX_TEST);
        gcmd(POS_TEST);
        gcmd(VEC_TEST);
#undef gcmd
    }
    printf("\nUnhandled write to GE addr:%08x sz:%d val:%08x", addr, sz, val);
}

static u32 read_results(struct NDS *this, u32 addr, u32 sz)
{
    if ((addr >= 0x04000620) && (addr < 0x04000630)) {
        u32 which = (addr - 0x04000620) >> 2;
        assert(which<4);
        return this->ge.results.pos_test[which];
    }
    if ((addr >= 0x04000630) && (addr < 0x04000638)) {
        u32 which = (addr - 0x04000630) >> 2;
        assert(which<2);
        return this->ge.results.vector[which];
    }
    if ((addr >= 0x04000640) && (addr < 0x04000680)) {
        u32 which = (addr - 0x04000640) >> 2;
        assert(which<16);
        return this->ge.matrices.clip.m[which];
    }
    if ((addr >= 0x04000680) && (addr < 0x040006A4)) {
        u32 which = (addr - 0x04000680) >> 2;
        assert(which<9);
        switch(which) {
            case 0: return this->ge.matrices.direction.m[0];
            case 1: return this->ge.matrices.direction.m[1];
            case 2: return this->ge.matrices.direction.m[2];
            case 3: return this->ge.matrices.direction.m[4];
            case 4: return this->ge.matrices.direction.m[5];
            case 5: return this->ge.matrices.direction.m[6];
            case 6: return this->ge.matrices.direction.m[8];
            case 7: return this->ge.matrices.direction.m[9];
            case 8: return this->ge.matrices.direction.m[10];
        }
    }
    printf("\nUNHANDLED READ FROM RESULTS AT %08x", addr);
    return 0;
}

u32 NDS_GE_read(struct NDS *this, u32 addr, u32 sz)
{
    u32 v;
    switch(addr) {
        case R9_GXSTAT:
            assert(sz==4);
            return GXSTAT.u;
    }
    if (addr >= 0x04000620) {
        assert(sz==4);
        return read_results(this, addr, sz);
    }
    printf("\nUnhandled read from GE addr:%08x sz:%d", addr, sz);
    return 0;
}

static void do_swap_buffers(void *ptr, u64 key, u64 current_clock, u32 jitter)
{
    struct NDS *this = (struct NDS *)ptr;
    this->ge.io.swap_buffers = 0;
    // TODO: this!
    this->ge.ge_has_buffer ^= 1;
    this->ge.buffers[this->ge.ge_has_buffer].polygon_index = 0;
    this->ge.buffers[this->ge.ge_has_buffer].vertex_index = 0;
    this->re.io.DISP3DCNT.poly_vtx_ram_overflow = 0;
    NDS_RE_render_frame(this);
}

void NDS_GE_vblank_up(struct NDS *this)
{
    // Schedule swap_buffers if we're scheduled to!
    if (this->ge.io.swap_buffers) {
        scheduler_only_add_abs(&this->scheduler, NDS_clock_current7(this) + 392, 0, this, &do_swap_buffers, NULL);
    }
    else {
        NDS_RE_render_frame(this);
    }
}
