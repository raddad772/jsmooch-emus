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

static u32 tbl_num_params[0xFF];
static i32 tbl_num_cycles[0xFF];
static u32 tbl_cmd_good[0xFF];

#define VTX_CACHE this->ge.params.vtx.cache
#define MS_COORD_DIR_PTR this->ge.matrices.stacks.coordinate_direction_ptr
#define MS_TEXTURE_PTR this->ge.matrices.stacks.texture_ptr
#define MS_PROJECTION_PTR this->ge.matrices.stacks.projection_ptr
#define MS_COORD this->ge.matrices.stacks.coordinate
#define MS_TEXTURE this->ge.matrices.stacks.texture
#define MS_PROJECTION this->ge.matrices.stacks.projection
#define MS_DIR this->ge.matrices.stacks.direction
#define M_COORD MS_COORD[MS_COORD_DIR_PTR & 31]
#define M_TEXTURE MS_TEXTURE[MS_TEXTURE_PTR & 1]
#define M_PROJECTION MS_PROJECTION[MS_PROJECTION_PTR & 1]
#define M_DIR MS_DIR[MS_COORD_DIR_PTR & 31]
#define M_CLIP this->ge.matrices.clip
#define M_SZ sizeof(struct NDS_GE_matrix)
#define GXSTAT this->ge.io.GXSTAT


static float vtx_to_float(i32 v)
{
    return ((float)v) / 4096.0f;
}

void NDS_GE_init(struct NDS *this)
{
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
    printf("\nNew matrix mode: %d", this->ge.io.MTX_MODE);
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
}

static void cmd_MTX_PUSH(struct NDS *this)
{
    // ClipMatrix = PositionMatrix * ProjectionMatrix
    printf("\nMTX_PUSH");
    u32 old_ptr;
    switch(this->ge.io.MTX_MODE) {
        case 0:
            old_ptr = MS_PROJECTION_PTR;
            MS_PROJECTION_PTR = (MS_PROJECTION_PTR + 1) & 1;
            GXSTAT.matrix_stack_over_or_underflow_error |= MS_PROJECTION_PTR == 1;
            GXSTAT.projection_matrix_stack_level = MS_PROJECTION_PTR;
            memcpy(&M_PROJECTION, &MS_PROJECTION[old_ptr], M_SZ);
            break;
        case 1:
        case 2:
            old_ptr = MS_COORD_DIR_PTR;

            MS_COORD_DIR_PTR = (MS_COORD_DIR_PTR + 1) & 63;
            GXSTAT.matrix_stack_over_or_underflow_error |= (MS_COORD_DIR_PTR & 31) == 31;
            GXSTAT.position_vector_matrix_stack_level = MS_COORD_DIR_PTR;
            memcpy(&M_COORD, &MS_COORD[old_ptr], M_SZ);
            memcpy(&M_DIR, &MS_DIR[old_ptr], M_SZ);
            break;
        case 3:
            old_ptr = MS_TEXTURE_PTR;
            MS_TEXTURE_PTR = (MS_TEXTURE_PTR + 1) & 1;
            GXSTAT.matrix_stack_over_or_underflow_error |= MS_TEXTURE_PTR == 1;
            memcpy(&M_TEXTURE, &MS_TEXTURE[old_ptr], M_SZ);
            // no texture matrix GXSTAT bits
            break;
    }
}

static void cmd_MTX_POP(struct NDS *this)
{
    printf("\nMTX_POP");
    i32 n = DATA[0];
    n = SIGNe6to32(n);
    switch(this->ge.io.MTX_MODE) {
        case 0:
            MS_PROJECTION_PTR = (MS_PROJECTION_PTR - n) & 1;
            GXSTAT.matrix_stack_over_or_underflow_error |= MS_PROJECTION_PTR == 0;
            GXSTAT.projection_matrix_stack_level = MS_PROJECTION_PTR;
            calculate_clip_matrix(this);
            break;
        case 1:
        case 2:
            MS_COORD_DIR_PTR = (MS_COORD_DIR_PTR - n) & 63;
            GXSTAT.matrix_stack_over_or_underflow_error |= (MS_COORD_DIR_PTR & 31) == 0;
            GXSTAT.position_vector_matrix_stack_level = MS_COORD_DIR_PTR;
            calculate_clip_matrix(this);
            break;
        case 3:
            MS_TEXTURE_PTR = (MS_TEXTURE_PTR - n) & 1;
            GXSTAT.matrix_stack_over_or_underflow_error |= MS_TEXTURE_PTR == 0;
            // no texture matrix GXSTAT bits
            break;
    }
}

static void cmd_MTX_STORE(struct NDS *this)
{
    // bit 0..4 is stack address. copy stack[S] to stack[N]
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

static inline struct NDS_GE_matrix *get_matrix_struct_for_mode(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0:
            return &M_PROJECTION;
        case 1:
            return &M_COORD;
        case 2:
            return &M_DIR;
        case 3:
            return &M_TEXTURE;
    }
    NOGOHERE;
}

static inline i32 *get_matrix_for_mode(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0:
            return M_PROJECTION.m;
        case 1:
            return M_COORD.m;
        case 2:
            return M_DIR.m;
        case 3:
            return M_TEXTURE.m;
    }
    NOGOHERE;
}

static void cmd_MTX_IDENTITY(struct NDS *this)
{
    struct NDS_GE_matrix *m = get_matrix_struct_for_mode(this);
    *m = (struct NDS_GE_matrix){{1 << 12, 0, 0, 0,
                                 0, 1 << 12, 0, 0,
                                 0, 0, 1 << 12, 0,
                                 0, 0, 0, 1 << 12
                                }};
}

static void cmd_MTX_LOAD_4x4(struct NDS *this)
{
    i32 *m = get_matrix_for_mode(this);
    for (u32 i = 0; i < 16; i++) {
        m[i] = (i32)DATA[i];
    }
}

static void cmd_MTX_LOAD_4x3(struct NDS *this)
{
    i32 *m = get_matrix_for_mode(this);
    for (u32 i = 0; i < 12; i++) {
        m[i] = (i32)DATA[i];
    }
    m[12] = m[13] = m[14] = 0;
    m[15] = 1 << 12;
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
    // note this modifies A
    i32 tmp_B[16];
    memcpy(tmp_B, B, sizeof(tmp_B));
    A[12] = A[13] = A[14] = 0;
    A[15] = 1 << 12;

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

static void pprint_matrix(i32 *s) {
    for (u32 y = 0; y < 16; y+=4) {
        printf("\n%f %f %f %f", vtx_to_float(s[y+0]), vtx_to_float(s[y+1]), vtx_to_float((s[y+2])), vtx_to_float(s[y+3]));
    }
}

static void matrix_multiply_3x3(i32 *A, i32 *B)
{
    // B(4x4) = A(3x3) * B(4x4)
    i32 tmp_A[16];
    i32 tmp_B[16];
    memcpy(tmp_B, B, sizeof(tmp_B));

    u32 i =0;
    for (u32 x = 0; x < 3; x++) {
        for (u32 y = 0; y < 12; y+=4) {
            tmp_A[y+x] = A[i];
        }
    }
    tmp_A[3] = tmp_A[7] = tmp_A[11] = 0;
    tmp_A[12] = tmp_A[13] = tmp_A[14] = 0;
    tmp_A[15] = 1 << 12;
    printf("\n\nMatrix 3x3. Matrix A: ");
    pprint_matrix(tmp_A);
    printf("\nMatrix B:");
    pprint_matrix(tmp_B);

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

    printf("\nOut matrix:");
    pprint_matrix(B);
}


static void cmd_MTX_MULT_4x4(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0: // projection
            matrix_multiply_4x4((i32 *)DATA, M_PROJECTION.m);
            calculate_clip_matrix(this);
            return;
        case 1: //  pos/coord matrix
            matrix_multiply_4x4((i32 *)DATA, M_COORD.m);
            calculate_clip_matrix(this);
            return;
        case 2: // both pos/coord and dir/vector matrices
            matrix_multiply_4x4((i32 *)DATA, M_COORD.m);
            matrix_multiply_4x4((i32 *)DATA, M_DIR.m);
            calculate_clip_matrix(this);
            return;
        case 3: // texture matrix
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


static void cmd_MTX_MULT_4x3(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0: // projection
            matrix_multiply_4x3((i32 *)DATA, M_PROJECTION.m);
            calculate_clip_matrix(this);
            return;
        case 1: //  pos/coord matrix
            matrix_multiply_4x3((i32 *)DATA, M_COORD.m);
            calculate_clip_matrix(this);
            return;
        case 2: // both pos/coord and dir/vector matrices
            matrix_multiply_4x3((i32 *)DATA, M_COORD.m);
            matrix_multiply_4x3((i32 *)DATA, M_DIR.m);
            calculate_clip_matrix(this);
            return;
        case 3: // texture matrix
            matrix_multiply_4x3((i32 *)DATA, M_TEXTURE.m);
            return;
    }
}

static void cmd_MTX_MULT_3x3(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0: // projection
            matrix_multiply_3x3((i32 *)DATA, M_PROJECTION.m);
            calculate_clip_matrix(this);
            return;
        case 1: //  pos/coord matrix
            matrix_multiply_3x3((i32 *)DATA, M_COORD.m);
            calculate_clip_matrix(this);
            return;
        case 2: // both pos/coord and dir/vector matrices
            matrix_multiply_3x3((i32 *)DATA, M_COORD.m);
            matrix_multiply_3x3((i32 *)DATA, M_DIR.m);
            calculate_clip_matrix(this);
            return;
        case 3: // texture matrix
            matrix_multiply_3x3((i32 *)DATA, M_TEXTURE.m);
            return;
    }
}

static void cmd_MTX_SCALE(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0:
            matrix_scale(M_PROJECTION.m, (i32 *)DATA);
            calculate_clip_matrix(this);
            return;
        case 1:
        case 2:
            matrix_scale(M_COORD.m, (i32 *)DATA);
            calculate_clip_matrix(this);
            return;
        case 3:
            matrix_scale(M_TEXTURE.m, (i32 *)DATA);
            return;
    }
}

static void cmd_MTX_TRANS(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0:
            matrix_translate(M_PROJECTION.m, (i32 *)DATA);
            calculate_clip_matrix(this);
            return;
        case 1:
            matrix_translate(M_COORD.m, (i32 *)DATA);
            calculate_clip_matrix(this);
            return;
        case 2:
            matrix_translate(M_COORD.m, (i32 *)DATA);
            matrix_translate(M_DIR.m, (i32 *)DATA);
            calculate_clip_matrix(this);
            return;
        case 3:
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
    printf("\nCOLOR r:%d g:%d b:%d", r, g, b);
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
    printf("\nTEXCOORD");
    // 1bit sign, 11bit integer, 4bit fraction
    this->ge.params.vtx.S = (i32)(i16)(DATA[0] & 0xFFFF);
    this->ge.params.vtx.T = (i32)(i16)(DATA[0] >> 16);
}

#define CCW 0
#define CW 1

static void matrix_multiply_by_vector(i32 *dest, i32 *matrix, i32 *src)
{
    // c[x] = a[x]*b1x + a[y]*b2x + a[y]*b3x + a[y]*b4x
    for (u32 x = 0; x < 4; x++) {
        dest[x] = src[x]*matrix[0+x];
        dest[x] += src[x]*matrix[4+x];
        dest[x] += src[x]*matrix[8+x];
        dest[x] += src[x]*matrix[12+x];
    }
}

static void transform_vertex_on_ingestion(struct NDS *this, struct NDS_GE_VTX *dest, struct NDS_GE_VTX *src)
{
    dest->color = this->ge.params.vtx.color;

    // TODO: add any I missed first pass
    if (this->ge.params.vtx.tex_param.texture_coord_transform_mode == NDS_TCTM_vertex) {
        for (u32 i = 0; i < 2; i++) {
            i64 x = src->xyzw[0] * M_TEXTURE.m[i*4]; // TODO: May have got x/y wrong here
            i64 y = src->xyzw[1] * M_TEXTURE.m[(i*4)+1];
            i64 z = src->xyzw[2] * M_TEXTURE.m[(i*4)+2];

            dest->uv[i] = (i16)(((x + y + z) >> 24) + src->uv[i]);
        }
    }
    matrix_multiply_by_vector(dest->xyzw, M_CLIP.m, src->xyzw);
    printf("\nTransformed vertex: %f %f %f", vtx_to_float(dest->xyzw[0]), vtx_to_float(dest->xyzw[1]), vtx_to_float(dest->xyzw[2]));
}

static void ingest_poly(struct NDS *this, u32 starting_sides) {
    struct NDS_GE_RE_POLY out;
    printf("\nRecv poly with %d sides", starting_sides);
    out.attr.u = this->ge.params.poly.current.attr.u;
    // TODO: add all relavent attributes to structure here

    // Get vertices
    // Transform them
    struct NDS_GE_VTX in_vertices[10]; // Max 10 vertices!
    for (u32 i = 0; i < starting_sides; i++) {
        transform_vertex_on_ingestion(this, &in_vertices[i], &VTX_CACHE.items[(VTX_CACHE.head + i) & 3]);
    }

    // Now clip...
    u32 needs_clipping = 0;
    for(u32 vn = 0; vn < starting_sides; vn++) {
        struct NDS_GE_VTX *v = &in_vertices[vn];
        i32 w = v->xyzw[3];

        for (int i = 0; i < 3; i++) {
            if (v->xyzw[i] < -w || v->xyzw[i] > w) {
                needs_clipping = 1;
                break;
            }
        }
        if (needs_clipping) break;
    }


    // Add vertices...
}

static void ingest_vertex(struct NDS *this, i32 x, i32 y, i32 z)
{
    // Add to vertex cache
    struct NDS_GE_VTX *o = &VTX_CACHE.items[VTX_CACHE.tail];
    o->xyzw[0] = x;
    o->xyzw[1] = y;
    o->xyzw[2] = z;
    o->uv[0] = this->ge.params.vtx.S;
    o->uv[1] = this->ge.params.vtx.T;

    VTX_CACHE.tail = (VTX_CACHE.tail + 1) & 3;
    VTX_CACHE.len++;

    switch(this->ge.params.vtx_strip.mode) {
        case NDS_GEM_SEPERATE_TRIANGLES:
            if (VTX_CACHE.len >= 3) {
                printf("\nDO POLY SEPARATE TRI");
                ingest_poly(this, 3);
                // clear the cache
                VTX_CACHE.tail = VTX_CACHE.head = 0;
                VTX_CACHE.len = 0;
            }
            break;
        case NDS_GEM_SEPERATE_QUADS:
            if (VTX_CACHE.len >= 4) {
                printf("\nDO POLY SEPARATE QUAD");
                ingest_poly(this, 4);
                VTX_CACHE.tail = VTX_CACHE.head = 0;
                VTX_CACHE.len = 0;
            }
            break;
        case NDS_GEM_TRIANGLE_STRIP:
            if (VTX_CACHE.len >= 3) {
                printf("\nDO POLY TRI STRIP");
                ingest_poly(this, 3);
                VTX_CACHE.head = (VTX_CACHE.head + 1) & 3;
                VTX_CACHE.len--;
            }
            break;
        case NDS_GEM_QUAD_STRIP:
            if (VTX_CACHE.len >= 4) {
                printf("\nDO POLY QUAD STRIP");
                ingest_poly(this, 4);
                VTX_CACHE.head = (VTX_CACHE.head + 1) & 3;
                VTX_CACHE.len--;
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
    printf("\nVTX_16: %f %f %f", vtx_to_float((i32)(i16)(DATA[0] & 0xFFFF)), vtx_to_float((i32)(i16)(DATA[0] >> 16)), vtx_to_float((i32)(i16)(DATA[1] & 0xFFFF)));
    ingest_vertex(this, (i32)(i16)(DATA[0] & 0xFFFF), (i32)(i16)(DATA[0] >> 16), (i32)(i16)(DATA[1] & 0xFFFF));
    //i32 x = DATA[0] &
}
//static void cmd_MTX_POP(struct NDS *this)

static void cmd_SWAP_BUFFERS(struct NDS *this)
{
    this->ge.io.swap_buffers = 1;
    this->ge.buffers[this->ge.ge_has_buffer].translucent_y_sorting_manual = DATA[0] & 1;
    this->ge.buffers[this->ge.ge_has_buffer].depth_buffering_w = (DATA[0] >> 1) & 1;
}

static void cmd_POLYGON_ATTR(struct NDS *this)
{
    printf("\nPOLYGON_ATTR");
    this->ge.params.poly.on_vtx_start.attr.u = DATA[0];
}

static void terminate_poly_strip(struct NDS *this)
{
    // TODO: ??
    VTX_CACHE.tail = VTX_CACHE.head = 0;
    VTX_CACHE.len = 0;
}

static void cmd_BEGIN_VTXS(struct NDS *this)
{
    printf("\nBEGIN_VTXS");
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
        //printf("\nSWAP BUFFERS PUSHEd, IGNORING FURTHER...");
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

    printf("\nHAndle cmd %02x", cmd);

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

u32 NDS_GE_read(struct NDS *this, u32 addr, u32 sz)
{
    u32 v;
    switch(addr) {
        case R9_GXSTAT:
            assert(sz==4);
            return GXSTAT.u;
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
    memset(this->ge.buffers[this->ge.ge_has_buffer].polygon, 0, sizeof(this->ge.buffers[this->ge.ge_has_buffer].polygon));
    memset(this->ge.buffers[this->ge.ge_has_buffer].vertex, 0, sizeof(this->ge.buffers[this->ge.ge_has_buffer].vertex));
    this->ge.buffers[this->ge.ge_has_buffer].polygon_index = 0;
    this->ge.buffers[this->ge.ge_has_buffer].vertex_index = 0;
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
