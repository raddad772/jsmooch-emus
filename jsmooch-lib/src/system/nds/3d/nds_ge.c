//
// Created by . on 3/9/25.
//

#include <stdlib.h>
#include <string.h>
#include "nds_ge.h"
#include "system/nds/nds_bus.h"
#include "system/nds/nds_irq.h"
#include "system/nds/nds_regs.h"
#include "system/nds/nds_dma.h"
#include "nds_3dmath.h"
#include "helpers/multisize_memaccess.c"

#define printfifo(...) (void)0
//#define printffifo(...) printf(__VA_ARGS__)
#define printfcd(...) (void)0
//#define printfcd(...) printf(__VA_ARGS__)

static u32 tbl_num_params[0xFF];
static i32 tbl_num_cycles[0xFF];
static u32 tbl_cmd_good[0xFF];

static float uv_to_float(i32 v)
{
    return ((float)v) / 16.0f;
}


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

#define DATA this->ge.cur_cmd.data

#define FIFO this->ge.fifo
#define VTX_LIST this->ge.params.vtx.input_list
#define MS_POSITION_VECTOR_PTR this->ge.matrices.stacks.position_vector_ptr
#define MS_TEXTURE_PTR this->ge.matrices.stacks.texture_ptr
#define MS_PROJECTION_PTR this->ge.matrices.stacks.projection_ptr
#define MS_POSITION this->ge.matrices.stacks.position
#define MS_TEXTURE this->ge.matrices.stacks.texture
#define MS_PROJECTION this->ge.matrices.stacks.projection
#define MS_VECTOR this->ge.matrices.stacks.vector
#define M_POSITION this->ge.matrices.position
#define M_TEXTURE this->ge.matrices.texture
#define M_PROJECTION this->ge.matrices.projection
#define M_VECTOR this->ge.matrices.vector
#define M_CLIP this->ge.matrices.clip
#define M_SZ sizeof(NDS_GE_matrix)
#define GXSTAT this->ge.io.GXSTAT

static void identity_matrix(i32 *m)
{
    memset(m, 0, sizeof(i32)*16);
    m[0] = m[5] = m[10] = m[15] = 1 << 12;
}



static void vertex_list_init(struct NDS_GE_VTX_list *l)
{
    l->pool_bitmap = 0;
    l->len = 0;
    l->first = NULL;
    l->last = NULL;
}

static struct NDS_GE_VTX_list_node *vertex_list_alloc_node(struct NDS_GE_VTX_list *l, u32 vtx_parent)
{
    if (l->len >= NDS_GE_VTX_LIST_MAX) {
        NOGOHERE;
        return NULL;
    }
    u32 poolbit = 1;
    u32 poolentry = 0;
    for (u32 i = 0; i < NDS_GE_VTX_LIST_MAX; i++) {
        if (!(l->pool_bitmap & poolbit)) break;
        poolentry += 1;
        poolbit <<= 1;
    }
    l->pool_bitmap |= poolbit;

    struct NDS_GE_VTX_list_node *a = &l->pool[poolentry];
    a->poolclear = ~poolbit;
    a->data.processed = 0;
    a->data.vtx_parent = vtx_parent;
    a->next = a->prev = NULL;
    return a;
}

static void vertex_list_free_node(struct NDS_GE_VTX_list *p, struct NDS_GE_VTX_list_node *n)
{
    p->pool_bitmap &= n->poolclear;
    n->next = n->prev = NULL;
}

static void vertex_list_swap_last_two(struct NDS_GE_VTX_list *l)
{
    struct NDS_GE_VTX_list_node *next_to_last = l->last->prev;
    struct NDS_GE_VTX_list_node *last = l->last;
    assert(l->first!=l->last);
    assert(l->first!=next_to_last);
    assert(l->last->next == NULL);
    next_to_last->prev->next = last;
    last->prev = next_to_last->prev;
    last->next = next_to_last;
    next_to_last->prev = last;
    next_to_last->next = NULL;
    l->last = next_to_last;
}

static void vertex_list_delete_first(struct NDS_GE_VTX_list *l)
{
    if (l->len == 0) return;
    if (l->len == 1) {
        l->first = l->last = NULL;
        l->pool_bitmap = 0;
        l->len = 0;
        return;
    }
    if (l->len == 2) {
        vertex_list_free_node(l, l->first);
        l->first = l->last;
        l->last->prev = NULL;
        l->len--;
        return;
    }
    struct NDS_GE_VTX_list_node *next = l->first->next;
    next->prev = NULL;
    vertex_list_free_node(l, l->first);
    l->first = next;
    l->len--;
}


static struct NDS_GE_VTX_list_node *vertex_list_add_to_end(struct NDS_GE_VTX_list *l, u32 vtx_parent)
{
    struct NDS_GE_VTX_list_node *n = vertex_list_alloc_node(l, vtx_parent);
    if (l->len == 0) {
        l->first = l->last = n;
    }
    else if (l->len == 1) {
        l->last = n;
        l->first->next = l->last;
        l->last->prev = l->first;
    }
    else {
        n->prev = l->last;
        l->last->next = n;
        l->last = n;
    }
    l->len++;
    assert(l->len < NDS_GE_VTX_LIST_MAX);
    return n;
}

static void copy_vertex_list_into(struct NDS_GE_VTX_list *dest, struct NDS_GE_VTX_list *src)
{
    vertex_list_init(dest);
    struct NDS_GE_VTX_list_node *src_node = src->first;
    i32 num = 0;
    while(src_node) {
        struct NDS_GE_VTX_list_node *dst_node = vertex_list_add_to_end(dest, num++);
        memcpy(&dst_node->data, &src_node->data, sizeof(struct NDS_GE_VTX_list_node_data));
        src_node = src_node->next;
    }
}

void NDS_GE_init(struct NDS *this) {
    vertex_list_init(&VTX_LIST);
    this->ge.ge_has_buffer = 0;
    for (u32 i = 0; i < 0xFF; i++) {
        tbl_num_params[i] = 0;
        tbl_num_cycles[i] = 0;
        tbl_cmd_good[i] = 0;
    }

#define SCMD(cmd_id, np, ncyc) tbl_num_params[NDS_GE_CMD_##cmd_id] = np; tbl_num_cycles[NDS_GE_CMD_##cmd_id] = ncyc; tbl_cmd_good[NDS_GE_CMD_##cmd_id] = 1
    SCMD(NOP, 0, 0);
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
    SCMD(PLTT_BASE, 1, 1); // 2B
    SCMD(DIF_AMB, 1, 4); // 30
    SCMD(SPE_EMI, 1, 4); //31
    SCMD(LIGHT_VECTOR, 1, 6); //32
    SCMD(LIGHT_COLOR, 1, 1); // 33
    SCMD(SHININESS, 32, 32); // 34
    SCMD(BEGIN_VTXS, 1, 1); // 40
    SCMD(END_VTXS, 0, 1); // 41
    SCMD(SWAP_BUFFERS, 1, 1); // 50
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
    identity_matrix(M_PROJECTION);
    identity_matrix(M_CLIP);
    identity_matrix(M_POSITION);
    identity_matrix(M_TEXTURE);
    identity_matrix(M_VECTOR);
    vertex_list_init(&VTX_LIST);
    FIFO.head = FIFO.tail = FIFO.len = 0;
    FIFO.pausing_cpu = 0;
    printfifo("\nWAITING CMD=1 (reset)");
    FIFO.waiting_for_cmd = 1;
}

static void fifo_update_len(struct NDS *this, u32 from_dma, u32 did_add)
{
    u32 old_bit = 0;
    if (GXSTAT.cmd_fifo_irq_mode) {
        old_bit = NDS_GE_check_irq(this);
    }

    u32 old_pausing_cpu = FIFO.pausing_cpu;
    FIFO.pausing_cpu = FIFO.len > 255;
    if (!old_pausing_cpu && FIFO.pausing_cpu) {
        //printf("\nFIFO PAUSE CPU");
    }
    else if (old_pausing_cpu && !FIFO.pausing_cpu) {
        //printf("\nFIFO UNPAUSE CPU!");
    }
    GXSTAT.cmd_fifo_len = FIFO.len > 255 ? 255 : FIFO.len;
    GXSTAT.cmd_fifo_less_than_half_full = FIFO.len < 128;
    GXSTAT.cmd_fifo_empty = FIFO.len == 0;

    if (GXSTAT.cmd_fifo_irq_mode) {
        u32 new_bit = NDS_GE_check_irq(this);
        if ((old_bit != new_bit) && new_bit)
            NDS_update_IF9(this, NDS_IRQ_GXFIFO);
    }

    if (!did_add && !from_dma && (FIFO.len < 128))
        NDS_trigger_dma9_if(this, NDS_DMA_GE_FIFO);
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

static void cmd_END_VTXS(struct NDS *this)
{

}

static void cmd_MTX_MODE(struct NDS *this)
{
    this->ge.io.MTX_MODE = DATA[0] & 3;
    printfcd("\nMTX_MODE(%d);", this->ge.io.MTX_MODE);
}

static void calculate_clip_matrix(struct NDS *this)
{
    this->ge.clip_mtx_dirty = 0;
    // Clip matrix = projection * position
    memcpy(M_CLIP, M_PROJECTION, M_SZ);
    matrix_multiply_4x4(M_CLIP, M_POSITION);
}

static void cmd_MTX_PUSH(struct NDS *this)
{
    // ClipMatrix = PositionMatrix * ProjectionMatrix
    u32 old_ptr;
    switch(this->ge.io.MTX_MODE) {
        case 0:
            memcpy(&MS_PROJECTION[MS_PROJECTION_PTR], &M_PROJECTION, M_SZ);
            MS_PROJECTION_PTR = (MS_PROJECTION_PTR + 1) & 1;
            GXSTAT.matrix_stack_over_or_underflow_error |= MS_PROJECTION_PTR == 1;
            GXSTAT.projection_matrix_stack_level = MS_PROJECTION_PTR;
            break;
        case 1:
        case 2:
            GXSTAT.matrix_stack_over_or_underflow_error |= MS_POSITION_VECTOR_PTR > 30;
            memcpy(&MS_POSITION[MS_POSITION_VECTOR_PTR & 31], &M_POSITION, M_SZ);
            memcpy(&MS_VECTOR[MS_POSITION_VECTOR_PTR & 31], &M_VECTOR, M_SZ);
            MS_POSITION_VECTOR_PTR = (MS_POSITION_VECTOR_PTR + 1) & 63;
            GXSTAT.position_vector_matrix_stack_level = MS_POSITION_VECTOR_PTR;
            break;
        case 3:
            memcpy(&MS_TEXTURE[MS_TEXTURE_PTR], &M_TEXTURE, M_SZ);
            MS_TEXTURE_PTR = (MS_TEXTURE_PTR + 1) & 1;
            GXSTAT.matrix_stack_over_or_underflow_error |= MS_TEXTURE_PTR == 1;
            //printfcd("\nPushed texture matrix:");
            //pprint_matrix(M_TEXTURE);
            // no texture matrix GXSTAT bits
            break;
    }
}

static void cmd_MTX_POP(struct NDS *this)
{
    u32 old_ptr;
    switch(this->ge.io.MTX_MODE) {
        case 0:
            GXSTAT.matrix_stack_over_or_underflow_error |= MS_PROJECTION_PTR == 0;
            MS_PROJECTION_PTR = (MS_PROJECTION_PTR - 1) & 1;
            GXSTAT.projection_matrix_stack_level = MS_PROJECTION_PTR;
            memcpy(&M_PROJECTION, &MS_PROJECTION[MS_PROJECTION_PTR], M_SZ);
            this->ge.clip_mtx_dirty = 1;
            break;
        case 1:
        case 2: {
            i32 n = ((i32)DATA[0] << 26) >> 26;
            MS_POSITION_VECTOR_PTR = (MS_POSITION_VECTOR_PTR - n) & 63;
            GXSTAT.matrix_stack_over_or_underflow_error |= (MS_POSITION_VECTOR_PTR) > 30;
            GXSTAT.position_vector_matrix_stack_level = MS_POSITION_VECTOR_PTR;
            memcpy(&M_POSITION, &MS_POSITION[MS_POSITION_VECTOR_PTR], M_SZ);
            memcpy(&M_VECTOR, &MS_VECTOR[MS_POSITION_VECTOR_PTR], M_SZ);
            this->ge.clip_mtx_dirty = 1;
            break; }
        case 3:
            GXSTAT.matrix_stack_over_or_underflow_error |= MS_TEXTURE_PTR == 0;
            MS_TEXTURE_PTR = (MS_TEXTURE_PTR - 1) & 1;
            memcpy(&M_TEXTURE, &MS_TEXTURE[MS_TEXTURE_PTR], M_SZ);
            // no texture matrix GXSTAT bits
            break;
    }
}

static void cmd_MTX_STORE(struct NDS *this)
{
    // bit 0..4 is stack address. copy stack[S] to stack[N]
    switch(this->ge.io.MTX_MODE) {
        case 0:
            memcpy(&MS_PROJECTION[0], &M_PROJECTION, M_SZ);
            break;
        case 1:
        case 2: {
            u32 n = DATA[0] & 0x1F;
            GXSTAT.matrix_stack_over_or_underflow_error |= n > 30;
            memcpy(&MS_VECTOR[n], &M_VECTOR, M_SZ);
            memcpy(&MS_POSITION[n], &M_POSITION, M_SZ);
            break; }
        case 3:
            memcpy(&MS_TEXTURE[0], &M_TEXTURE, M_SZ);
            break;
    }
}

static void cmd_MTX_RESTORE(struct NDS *this)
{
    // bit 0..4 is stack address. copy stack[N] to stack[S]
    switch(this->ge.io.MTX_MODE) {
        case 0:
            memcpy(&M_PROJECTION, &MS_PROJECTION[0], M_SZ);
            this->ge.clip_mtx_dirty = 1;
            break;
        case 1:
        case 2: {
            u32 n = DATA[0] & 0x1F;
            GXSTAT.matrix_stack_over_or_underflow_error |= n > 30;
            memcpy(&M_VECTOR, &MS_VECTOR[n], M_SZ);
            memcpy(&M_POSITION, &MS_POSITION[n], M_SZ);
            this->ge.clip_mtx_dirty = 1;
            break; }
        case 3:
            memcpy(&M_TEXTURE, &MS_TEXTURE[0], M_SZ);
            break;
    }
}

static void cmd_MTX_IDENTITY(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0:
            identity_matrix(M_PROJECTION);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 1:
            identity_matrix(M_POSITION);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 2:
            identity_matrix(M_POSITION);
            identity_matrix(M_VECTOR);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 3:
            identity_matrix(M_TEXTURE);
            return;
    }
}

static void cmd_MTX_LOAD_4x4(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0:
            matrix_load_4x4(M_PROJECTION, (i32 *) DATA);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 1:
            matrix_load_4x4(M_POSITION, (i32 *) DATA);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 2:
            matrix_load_4x4(M_POSITION, (i32 *) DATA);
            matrix_load_4x4(M_VECTOR, (i32 *) DATA);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 3:
            matrix_load_4x4(M_TEXTURE, (i32 *) DATA);
            return;
    }
}

static void cmd_MTX_LOAD_4x3(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0:
            matrix_load_4x3(M_PROJECTION, (i32 *) DATA);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 1:
            matrix_load_4x3(M_POSITION, (i32 *) DATA);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 2:
            matrix_load_4x3(M_POSITION, (i32 *) DATA);
            matrix_load_4x3(M_VECTOR, (i32 *) DATA);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 3:
            matrix_load_4x3(M_TEXTURE, (i32 *) DATA);
            return;
    }
}

static void cmd_SHININESS(struct NDS *this) {
    u32 i = 0;
    for (u32 p = 0; p < 32; p++) {
        u32 word = DATA[p];
        for (u32 w = 0; w < 4; w++) {
            this->re.io.SHININESS[i++] = word & 0xFF;
            word >>= 8;
        }
    }
};

static void cmd_MTX_MULT_4x4(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0: // projection
            matrix_multiply_4x4(M_PROJECTION, (i32 *)DATA);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 1: //  pos/coord matrix
            matrix_multiply_4x4(M_POSITION, (i32 *)DATA);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 2: // both pos/coord and dir/vector matrices
            matrix_multiply_4x4(M_POSITION, (i32 *)DATA);
            matrix_multiply_4x4(M_VECTOR, (i32 *)DATA);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 3: // texture matrix
            matrix_multiply_4x4(M_TEXTURE, (i32 *)DATA);
            return;
    }
}

static void cmd_MTX_MULT_4x3(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0: // projection
            printfcd("\nMTX_MULT_4x3(Projection);");
            this->ge.clip_mtx_dirty = 1;
            return;
        case 1: //  pos/coord matrix
            matrix_multiply_4x3(M_POSITION, (i32 *)DATA);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 2: // both pos/coord and dir/vector matrices
            matrix_multiply_4x3(M_POSITION, (i32 *)DATA);
            matrix_multiply_4x3(M_VECTOR, (i32 *)DATA);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 3: // texture matrix
            matrix_multiply_4x3(M_TEXTURE, (i32 *)DATA);
            printfcd("\nMTX_MULT_4x3(texture);");
            return;
    }
}

static void cmd_MTX_MULT_3x3(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0: // projection
            matrix_multiply_3x3(M_PROJECTION, (i32 *)DATA);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 1: //  pos/coord matrix
            matrix_multiply_3x3(M_POSITION, (i32 *)DATA);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 2: // both pos/coord and dir/vector matrices
            matrix_multiply_3x3(M_POSITION, (i32 *)DATA);
            matrix_multiply_3x3(M_VECTOR, (i32 *)DATA);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 3: // texture matrix
            matrix_multiply_3x3(M_TEXTURE, (i32 *)DATA);
            return;
    }
}

static void cmd_MTX_SCALE(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0:
            matrix_scale(M_PROJECTION, (i32 *)DATA);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 1:
        case 2:
            matrix_scale(M_POSITION, (i32 *)DATA);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 3:
            matrix_scale(M_TEXTURE, (i32 *)DATA);
            return;
    }
}

static void cmd_MTX_TRANS(struct NDS *this)
{
    switch(this->ge.io.MTX_MODE) {
        case 0:
            matrix_translate(M_PROJECTION, (i32 *)DATA);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 1:
            matrix_translate(M_POSITION, (i32 *)DATA);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 2:
            matrix_translate(M_POSITION, (i32 *)DATA);
            matrix_translate(M_VECTOR, (i32 *)DATA);
            this->ge.clip_mtx_dirty = 1;
            return;
        case 3:
            matrix_translate(M_TEXTURE, (i32 *)DATA);
            return;
    }
}

static void cmd_COLOR(struct NDS *this)
{
    u32 color_in = DATA[0];
    u32 r = (color_in >> 0) & 0x1F;
    u32 g = (color_in >> 5) & 0x1F;
    u32 b = (color_in >> 10) & 0x1F;
    this->ge.params.vtx.color[0] = r;
    this->ge.params.vtx.color[1] = g;
    this->ge.params.vtx.color[2] = b;
}

static void cmd_NORMAL(struct NDS *this)
{
    this->ge.lights.normal[0] = (i16)((DATA[0] & 0x000003FF) << 6) >> 6;
    this->ge.lights.normal[1] = (i16)((DATA[0] & 0x000FFC00) >> 4) >> 6;
    this->ge.lights.normal[2] = (i16)((DATA[0] & 0x3FF00000) >> 14) >> 6;

    if (this->ge.params.poly.current.tex_param.texture_coord_transform_mode ==  NDS_TCTM_normal) {
        this->ge.params.vtx.uv[0] = this->ge.params.vtx.original_uv[0] + (((i64) this->ge.lights.normal[0]*M_TEXTURE[0] + (i64) this->ge.lights.normal[1]*M_TEXTURE[4] + (i64) this->ge.lights.normal[2]*M_TEXTURE[8]) >> 21);
        this->ge.params.vtx.uv[1] = this->ge.params.vtx.original_uv[1] + (((i64) this->ge.lights.normal[0]*M_TEXTURE[1] + (i64) this->ge.lights.normal[1]*M_TEXTURE[5] + (i64) this->ge.lights.normal[2]*M_TEXTURE[9]) >> 21);
    }

    i32 normaltrans[3]; // should be 1 bit sign 10 bits frac
    normaltrans[0] = ((this->ge.lights.normal[0]*M_VECTOR[0] + this->ge.lights.normal[1]*M_VECTOR[4] + this->ge.lights.normal[2]*M_VECTOR[8]) << 9) >> 21;
    normaltrans[1] = ((this->ge.lights.normal[0]*M_VECTOR[1] + this->ge.lights.normal[1]*M_VECTOR[5] + this->ge.lights.normal[2]*M_VECTOR[9]) << 9) >> 21;
    normaltrans[2] = ((this->ge.lights.normal[0]*M_VECTOR[2] + this->ge.lights.normal[1]*M_VECTOR[6] + this->ge.lights.normal[2]*M_VECTOR[10]) << 9) >> 21;

    i32 c = 0;
    u32 vtxbuff[3] = {(u32)this->ge.lights.material_color.emission[0] << 14, (u32)this->ge.lights.material_color.emission[1] << 14, (u32)this->ge.lights.material_color.emission[2] << 14 };
    for (int i = 0; i < 4; i++)
    {
        if (!(this->ge.params.poly.current.attr.u & (1<<i)))
            continue;

        i32 dot = ((this->ge.lights.light[i].direction[0]*normaltrans[0]) >> 9) + ((this->ge.lights.light[i].direction[1]*normaltrans[1]) >> 9) + ((this->ge.lights.light[i].direction[2]*normaltrans[2]) >> 9);

        i32 shinelevel;
        if (dot > 0) {
            i32 diffdot = (dot << 21) >> 21;
            vtxbuff[0] += (this->ge.lights.material_color.diffuse[0] * this->ge.lights.light[i].color[0] * diffdot) & 0xFFFFF;
            vtxbuff[1] += (this->ge.lights.material_color.diffuse[1] * this->ge.lights.light[i].color[1] * diffdot) & 0xFFFFF;
            vtxbuff[2] += (this->ge.lights.material_color.diffuse[2] * this->ge.lights.light[i].color[2] * diffdot) & 0xFFFFF;

            dot += normaltrans[2];

            dot = (dot << 21) >> 21;
            dot = ((dot * dot) >> 10) & 0x3FF;

            shinelevel = ((dot * this->ge.lights.material_color.reciprocal[i]) >> 8) - (1<<9);

            if (shinelevel < 0) shinelevel = 0;
            else {
                shinelevel = (shinelevel << 18) >> 18;
                if (shinelevel < 0) shinelevel = 0;
                else if (shinelevel > 0x1FF) shinelevel = 0x1FF;
            }
        }
        else shinelevel = 0;

        if (this->ge.lights.shininess_enable)
        {
            shinelevel >>= 2;
            shinelevel = this->re.io.SHININESS[shinelevel];
            shinelevel <<= 1;
        }

        vtxbuff[0] += ((this->ge.lights.material_color.reflection[0] * shinelevel) + (this->ge.lights.material_color.ambient[0] << 9)) * this->ge.lights.light[i].color[0];
        vtxbuff[1] += ((this->ge.lights.material_color.reflection[1] * shinelevel) + (this->ge.lights.material_color.ambient[1] << 9)) * this->ge.lights.light[i].color[1];
        vtxbuff[2] += ((this->ge.lights.material_color.reflection[2] * shinelevel) + (this->ge.lights.material_color.ambient[2] << 9)) * this->ge.lights.light[i].color[2];

        c++;
    }

    this->ge.params.vtx.color[0] = (vtxbuff[0] >> 14 > 31) ? 31 : (vtxbuff[0] >> 14);
    this->ge.params.vtx.color[1] = (vtxbuff[1] >> 14 > 31) ? 31 : (vtxbuff[1] >> 14);
    this->ge.params.vtx.color[2] = (vtxbuff[2] >> 14 > 31) ? 31 : (vtxbuff[2] >> 14);
}

static void cmd_TEXCOORD(struct NDS *this)
{
    this->ge.params.vtx.original_uv[0] = DATA[0] & 0xFFFF;
    this->ge.params.vtx.original_uv[1] = DATA[1] >> 16;
    if (this->ge.params.poly.current.tex_param.texture_coord_transform_mode == NDS_TCTM_texcoord) {
        printf("\nTEXCOORD MTX!");
        this->ge.params.vtx.uv[0] = (this->ge.params.vtx.original_uv[0]*M_TEXTURE[0] + this->ge.params.vtx.original_uv[1]*M_TEXTURE[4] + M_TEXTURE[8] + M_TEXTURE[12]) >> 12;
        this->ge.params.vtx.uv[1] = (this->ge.params.vtx.original_uv[0]*M_TEXTURE[1] + this->ge.params.vtx.original_uv[1]*M_TEXTURE[5] + M_TEXTURE[9] + M_TEXTURE[13]) >> 12;
    }
    else
    {
        this->ge.params.vtx.uv[0] = this->ge.params.vtx.original_uv[0];
        this->ge.params.vtx.uv[1] = this->ge.params.vtx.original_uv[1];
    }
}

#define CCW 0
#define CW 1

static inline u32 C5to6(u32 c) {
    return c ? ((c << 1) + 1) : 0;
}

static void transform_vertex_on_ingestion(struct NDS *this, struct NDS_GE_VTX_list_node *node)
{
    if (this->ge.clip_mtx_dirty) calculate_clip_matrix(this);
    node->data.processed = 0;
    node->data.color[0] = C5to6(this->ge.params.vtx.color[0]);
    node->data.color[1] = C5to6(this->ge.params.vtx.color[1]);
    node->data.color[2] = C5to6(this->ge.params.vtx.color[2]);
    node->data.processed = 0;

    i64 tmp[4] = {(i64)this->ge.params.vtx.x, (i64)this->ge.params.vtx.y, (i64)this->ge.params.vtx.z, 1 << 12};
    node->data.xyzw[0] = (i32)((tmp[0]*M_CLIP[0] + tmp[1]*M_CLIP[4] + tmp[2]*M_CLIP[8] + tmp[3]*M_CLIP[12]) >> 12);
    node->data.xyzw[1] = (i32)((tmp[0]*M_CLIP[1] + tmp[1]*M_CLIP[5] + tmp[2]*M_CLIP[9] + tmp[3]*M_CLIP[13]) >> 12);
    node->data.xyzw[2] = (i32)((tmp[0]*M_CLIP[2] + tmp[1]*M_CLIP[6] + tmp[2]*M_CLIP[10] + tmp[3]*M_CLIP[14]) >> 12);
    node->data.xyzw[3] = (i32)((tmp[0]*M_CLIP[3] + tmp[1]*M_CLIP[7] + tmp[2]*M_CLIP[11] + tmp[3]*M_CLIP[15]) >> 12);

    if (this->ge.params.poly.current.tex_param.texture_coord_transform_mode == NDS_TCTM_vertex) {
        node->data.uv[0] = ((tmp[0]*M_TEXTURE[0] + tmp[1]*M_TEXTURE[4] + tmp[2]*M_TEXTURE[8]) >> 24) + this->ge.params.vtx.original_uv[0];
        node->data.uv[1] = ((tmp[0]*M_TEXTURE[1] + tmp[1]*M_TEXTURE[5] + tmp[2]*M_TEXTURE[9]) >> 24) + this->ge.params.vtx.original_uv[1];
    }
    else {
        node->data.uv[0] = this->ge.params.vtx.uv[0];
        node->data.uv[1] = this->ge.params.vtx.uv[1];
    }
}

static u32 clip_against_plane(struct NDS *this, i32 axis, u32 is_compare_GT, struct NDS_GE_VTX_list *vertex_list_in, struct NDS_GE_VTX_list *vertex_list_out) {
    const i32 clip_precision = 18;

    const u32 size = vertex_list_in->len;
    
    struct NDS_GE_VTX_list_node *v0 = NULL;

    u32 clipped = 0;
    // LT = x < -w
    // GT = x > w
    for (int i = 0; i < size; i++) {
        if (!v0) v0 = vertex_list_in->first;
        else v0 = v0->next;

        u32 comp;
        if (is_compare_GT)
            comp = v0->data.xyzw[axis] > v0->data.xyzw[3];
        else
            comp = v0->data.xyzw[axis] < -v0->data.xyzw[3];
        if (comp) {
            struct NDS_GE_VTX_list_node *vlns[2] = {v0->prev, v0->next};
            if (!vlns[0]) vlns[0] = vertex_list_in->last;
            if (!vlns[1]) vlns[1] = vertex_list_in->first;
            for (u32 j = 0; j < 2; j++) {
                struct NDS_GE_VTX_list_node *v1 = vlns[j];

                if (is_compare_GT)
                    comp = v1->data.xyzw[axis] > v1->data.xyzw[3];
                else
                    comp = v1->data.xyzw[axis] < -v1->data.xyzw[3];
                if (!comp) {
                    const i64 sign = v0->data.xyzw[axis] < -v0->data.xyzw[3] ? 1 : -1;
                    const i64 numer = v1->data.xyzw[axis] + sign * v1->data.xyzw[3];
                    const i64 denom = (v0->data.xyzw[3] - v1->data.xyzw[3]) +
                                      (v0->data.xyzw[axis] - v1->data.xyzw[axis]) * sign;
                    const i64 scale = -sign * ((i64) numer << clip_precision) / denom;
                    const i64 scale_inv = (1 << clip_precision) - scale;

                    struct NDS_GE_VTX_list_node *clipped_vertex = vertex_list_add_to_end(vertex_list_out, 0);

                    for (u32 k = 0; k < 4; k++) {
                        clipped_vertex->data.xyzw[k] = (v1->data.xyzw[k] * scale_inv + v0->data.xyzw[k] * scale) >> clip_precision;
                        clipped_vertex->data.color[k] = (v1->data.color[k] * scale_inv + v0->data.color[k] * scale) >> clip_precision;
                    }

                    for (u32 k = 0; k < 2; k++) {
                        clipped_vertex->data.uv[k] = (v1->data.uv[k] * scale_inv + v0->data.uv[k] * scale) >> clip_precision;
                    }
                }
            }
            clipped = 1;
        } else {
            struct NDS_GE_VTX_list_node *n = vertex_list_add_to_end(vertex_list_out, 0);
            memcpy(&n->data, &v0->data, sizeof(struct NDS_GE_VTX_list_node_data));
        }
    }

    return clipped;
}

static void clip_verts(struct NDS *this, struct NDS_RE_POLY *out)
{
    static struct NDS_GE_VTX_list tmp;
    vertex_list_init(&tmp);

#define COMPARE_GT 1
#define COMPARE_LT 0

    u32 far_plane_intersecting = clip_against_plane(this, 2, COMPARE_GT, &out->vertex_list, &tmp);

    if(!out->attr.render_if_intersect_far_plane && far_plane_intersecting) {
        vertex_list_init(&out->vertex_list);
        return;
    }
    vertex_list_init(&out->vertex_list);
    clip_against_plane(this, 2, COMPARE_LT, &tmp, &out->vertex_list);
    vertex_list_init(&tmp);

    clip_against_plane(this, 1, COMPARE_GT, &out->vertex_list, &tmp);
    vertex_list_init(&out->vertex_list);
    clip_against_plane(this, 1, COMPARE_LT, &tmp, &out->vertex_list);
    vertex_list_init(&tmp);

    clip_against_plane(this, 0, COMPARE_GT, &out->vertex_list, &tmp);
    vertex_list_init(&out->vertex_list);
    clip_against_plane(this, 0, COMPARE_LT, &tmp, &out->vertex_list);
}

static u32 determine_needs_clipping(struct NDS_GE_VTX_list_node *v)
{
    if (v->data.processed) return 0;
    i32 w = v->data.xyzw[3];

    for (int i = 0; i < 3; i++) {
        if (v->data.xyzw[i] < -w || v->data.xyzw[i] > w) {
            return 1;
        }
    }
    return 0;
}

static u32 commit_vertex(struct NDS *this, struct NDS_GE_VTX_list_node *v, i32 xx, i32 yy, i32 zz, i32 ww, i32 *uv, u32 cr, u32 cg, u32 cb)
{
    struct NDS_GE_BUFFERS *b = &this->ge.buffers[this->ge.ge_has_buffer];
    if (this->ge.ge_has_buffer > 1) {
        printf("\nWOAH! cyc:%lld", this->clock.master_cycle_count7+this->waitstates.current_transaction);
    }
    u32 addr = b->vertex_index;

    /*b->vertex_index++;
    if (b->vertex_index >= 6144) {
        this->re.io.DISP3DCNT.poly_vtx_ram_overflow = 1;
    }

    struct NDS_RE_VERTEX *v = &b->vertex[addr];*/

    // We need...xx, yy, zz, ww, uv, and color
    // 32 bits each = 24 bytes...
    v->data.xyzw[0] = xx;
    v->data.xyzw[1] = yy;
    v->data.xyzw[2] = zz;
    v->data.xyzw[3] = ww;
    v->data.uv[0] = uv[0];
    v->data.uv[1] = uv[1];
    v->data.color[0] = cr;
    v->data.color[1] = cg;
    v->data.color[2] = cb;
    return addr;
}

static void normalize_w(struct NDS *this, struct NDS_RE_POLY *out)
{
    out->w_normalization_left = 0;
    out->w_normalization_right = 0;
    u32 longest_one = 0;
    struct NDS_GE_VTX_list_node *item = out->vertex_list.first;
    while(item) {
        u32 w = item->data.xyzw[3];
        u32 leading = __builtin_clrsb(w);
        u32 how_many_significant = 32 - leading;
        if (how_many_significant > longest_one) longest_one = how_many_significant;
        item = item->next;
    }

    // We either need to cut them down so the longest one is only 16 bits,
    // or boost them up so the shortest one is 16 bits
    if (longest_one > 16) {
        // Rown UP
        out->w_normalization_right = (((longest_one - 16) + 3) >> 2) << 2;
    }
    else {
        // round UP
        out->w_normalization_left = (((16 - longest_one) + 0) >> 2) << 2;
    }

    //printf("\n\nW NORMALIZE!");
    item = out->vertex_list.first;
    while(item) {
        if (out->w_normalization_right) item->data.w_normalized = item->data.xyzw[3] >> out->w_normalization_right;
        else if (out->w_normalization_left) item->data.w_normalized = item->data.xyzw[3] << out->w_normalization_left;
        else item->data.w_normalized = item->data.xyzw[3];
        //printf("\nORIGINAL W: %04x NEW W:%04x sl:%d sr:%d", item->data.xyzw[3], item->data.w_normalized, out->w_normalization_left, out->w_normalization_right);
        item = item->next;
    }
}

static void finalize_verts_and_get_first_addr(struct NDS *this, struct NDS_RE_POLY *poly)
{
    // Final transform to screen and write into vertex buffer
    struct NDS_GE_BUFFERS *b = &this->ge.buffers[this->ge.ge_has_buffer];

    //printf("\nFinalzie verts");
    // Go thru verts...
    struct NDS_GE_VTX_list_node *first_node = poly->vertex_list.first;
    struct NDS_GE_VTX_list_node *node = first_node;
    u32 num = 0;
    while(node) {
        if (!node->data.processed) {
            node->data.processed = 1;

            i32 x = node->data.xyzw[0];
            i32 y = node->data.xyzw[1];
            i32 z = node->data.xyzw[2];
            i32 w = node->data.xyzw[3];

            w &= 0xFFFFFF;
            i32 scrX, scrY;
            if (w > 0) {
                scrX = x + w;
                scrY = -y + w;
                //sscrY = y + w;
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
            node->data.vram_ptr = commit_vertex(this, node, scrX, scrY, node->data.xyzw[2], node->data.xyzw[3] & 0xFFFFFF, node->data.uv, node->data.color[0], node->data.color[1], node->data.color[2]);
        }
        //printf("\nVert num %d: %d %d, s:%d/%f t:%d/%f", num++, node->data.xyzw[0], node->data.xyzw[1], node->data.uv[0], uv_to_float(node->data.uv[0]), node->data.uv[1], uv_to_float(node->data.uv[1]));
        node = node->next;
    }
}

static u32 edge_is_top_or_bottom(struct NDS_RE_POLY *poly, struct NDS_GE_VTX_list_node *v[])
{
    u32 y1, y2;
    if (v[0]->data.xyzw[1] > v[1]->data.xyzw[1]) {
        y1 = v[1]->data.xyzw[1];
        y2 = v[0]->data.xyzw[1];
    }
    else {
        y1 = v[0]->data.xyzw[1];
        y2 = v[1]->data.xyzw[1];
    }
    return v[0]->data.xyzw[1] < v[1]->data.xyzw[1];
}

static void determine_highest_vertex(struct NDS_RE_POLY *poly, struct NDS_GE_BUFFERS *b)
{
    poly->min_y = 512;
    poly->max_y = 0;
    struct NDS_GE_VTX_list_node *v = poly->vertex_list.first;
    poly->highest_vertex = v;
    while(v) {
        if (v->data.xyzw[1] < poly->highest_vertex->data.xyzw[1]) poly->highest_vertex = v;
        if (v->data.xyzw[1] < poly->min_y) poly->min_y = v->data.xyzw[1];
        if (v->data.xyzw[1] > poly->max_y) poly->max_y = v->data.xyzw[1];
        v = v->next;
    }
}

static u32 determine_winding_order(struct NDS_RE_POLY *poly, struct NDS_GE_BUFFERS *b)
{
    // OK, now progress in order until we have a y-delta of >0.
    struct NDS_GE_VTX_list_node *n = poly->highest_vertex;
    struct NDS_GE_VTX_list_node *n_comp = n;
    for (u32 i = 1; i < poly->vertex_list.len; i++) {
        n_comp = n_comp->next;
        if (!n_comp) n_comp = poly->vertex_list.first;
        i32 diff = (i32)n_comp->data.xyzw[0] - n->data.xyzw[0];
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
    struct NDS_GE_VTX_list_node *v[2];
    v[1] = poly->vertex_list.first;
    u32 edgenum = 0;

    /*printf("\n\nPOLY!");
    for (u32 i = 0; i < poly->num_vertices; i++) {
        struct NDS_RE_VERTEX *pver = &b->vertex[poly->vertex_pointers[i]];
        printf("\nvert%d: x:%f y:%f color:%04x s:%f t:%f", poly->vertex_pointers[i], vtx_to_float(pver->xx), vtx_to_float(pver->yy), pver->color, uv_to_float(pver->uv[0]), uv_to_float(pver->uv[1]));
    }*/

    determine_highest_vertex(poly, b);

    //poly->winding_order = winding_order;
    poly->edge_r_bitfield = 0;

    /*for (u32 i = 1; i <= poly->vertex_list.len; i++) {
        v[0] = v[1];
        v[1] = v[0]->next;
        if (!v[1]) v[1] = poly->vertex_list.first;
        u32 top_to_bottom = edge_is_top_or_bottom(poly, v) ^ 1;

        //printf("\nV0 %d,%d V1 %d,%d top_to_bottom:%d", v[0]->xx, v[0]->yy, v[1]->xx, v[1]->yy, top_to_bottom);
        if (poly->winding_order == CCW) {
            //printf("\nTOP TO BOTTOM? %d")
            // top to bottom means left edge on CCW
            if (!top_to_bottom) poly->edge_r_bitfield |= (1 << edgenum);
        }
        else {
            // top to bottom means right edge on CW
            if (top_to_bottom) poly->edge_r_bitfield |= (1 << edgenum);
        }
        edgenum++;
    }*/

    v[0] = v[1];
    v[1] = poly->vertex_list.first;
    u32 top_to_bottom = edge_is_top_or_bottom(poly, v);
    if (poly->winding_order == CCW) {
        // top to bottom means left edge on CCW
        if (!top_to_bottom) poly->edge_r_bitfield |= (1 << edgenum);
    }
    else {
        // top to bottom means right edge on CW
        if (top_to_bottom) poly->edge_r_bitfield |= (1 << edgenum);
    }
}

static void list_reverse(struct NDS_GE_VTX_list *l)
{
    struct NDS_GE_VTX_list_node *node = l->first;
    l->first= l->last;
    l->last = node;
    while(node) {
        struct NDS_GE_VTX_list_node *t = node->next;

        node->next = node->prev;
        node->prev = t;

        node = t;
    }
}

static void ingest_poly(struct NDS *this, u32 winding_order) {
    struct NDS_GE_BUFFERS *b = &this->ge.buffers[this->ge.ge_has_buffer];
    u32 addr = b->polygon_index;

    struct NDS_RE_POLY *out = &b->polygon[addr];
    out->vertex_mode = this->ge.params.vtx_strip.mode;
    out->sampler.filled_out = 0;
    out->attr.u = this->ge.params.poly.current.attr.u;
    out->tex_param.u = this->ge.params.poly.current.tex_param.u;
    out->pltt_base = this->ge.params.poly.current.pltt_base;
    // TODO: add all relavent attributes to structure here

    copy_vertex_list_into(&out->vertex_list, &VTX_LIST);
    if (this->ge.params.vtx_strip.mode == NDS_GEM_QUAD_STRIP) {
        vertex_list_swap_last_two(&out->vertex_list);
    }

    struct NDS_GE_VTX_list_node *v0 = out->vertex_list.first;
    struct NDS_GE_VTX_list_node *v1 = v0->next;
    struct NDS_GE_VTX_list_node *v2 = v1->next;

    i64 normalX = ((i64) (v0->data.xyzw[1] - v1->data.xyzw[1]) * (v2->data.xyzw[3] - v1->data.xyzw[3]))
                  - ((i64) (v0->data.xyzw[3] - v1->data.xyzw[3]) * (v2->data.xyzw[1] - v1->data.xyzw[1]));
    i64 normalY = ((i64) (v0->data.xyzw[3] - v1->data.xyzw[3]) * (v2->data.xyzw[0] - v1->data.xyzw[0]))
                  - ((i64) (v0->data.xyzw[0] - v1->data.xyzw[0]) * (v2->data.xyzw[3] - v1->data.xyzw[3]));
    i64 normalZ = ((i64) (v0->data.xyzw[0] - v1->data.xyzw[0]) * (v2->data.xyzw[1] - v1->data.xyzw[1]))
                  - ((i64) (v0->data.xyzw[1] - v1->data.xyzw[1]) * (v2->data.xyzw[0] - v1->data.xyzw[0]));

    // shift until both zero or 1
    while ((((normalX >> 31) ^ (normalX >> 63)) != 0) ||
           (((normalY >> 31) ^ (normalY >> 63)) != 0) ||
           (((normalZ >> 31) ^ (normalZ >> 63)) != 0)) {
        normalX >>= 4;
        normalY >>= 4;
        normalZ >>= 4;
    }

    i64 dot = ((i64) v1->data.xyzw[0] * normalX) + ((i64) v1->data.xyzw[1] * normalY) +
              ((i64) v1->data.xyzw[3] * normalZ);

    out->front_facing = (dot <= 0);

    u32 frontface = (dot < 0) ^ winding_order;
    u32 backface = (dot > 0) ^ winding_order;

    if (frontface && (!out->attr.render_front)) {
        return;
    } else if (backface && (!out->attr.render_back)) {
        return;
    }

    // Now clip...
    u32 needs_clipping = 0;
    struct NDS_GE_VTX_list_node *n = out->vertex_list.first;
    while (n) {
        needs_clipping = determine_needs_clipping(n);
        if (needs_clipping) break;
        n = n->next;
    }

    // Clip vertices!
    if (needs_clipping) {
        clip_verts(this, out);
    }
    if (out->vertex_list.len < 3) {
        // Whole poly outside view...
        return;
    }

    // GO through leafs.
    // For any unprocessed vertices, add to polygorm RAM.
    if (out->vertex_list.len > 10) {
        printf("\nABORT FOR >10 VERTS IN A POLY?!");
        return;
    }

    finalize_verts_and_get_first_addr(this, out);
    normalize_w(this, out);
    if (this->re.io.DISP3DCNT.poly_vtx_ram_overflow) {
        return;
    }


    //printf("\n\nEvaluate for poly %d", addr);
    evaluate_edges(this, out, winding_order);
    /*if ((!out->attr.render_back && !out->front_facing) ||
        (!out->attr.render_front && out->front_facing)) {
        b->polygon_index--;
        return;
    }*/

    b->polygon_index++;
    if (b->polygon_index >= 2048) {
        this->re.io.DISP3DCNT.poly_vtx_ram_overflow = 1;
    }
    //printf("\npoly %d sides:%d front_facing:%d", addr, out->num_vertices, out->front_facing);
    printfcd("\nOUTPUT POLY HAS %d SIDES", out->vertex_list.len);
}

static void ingest_vertex(struct NDS *this) {
    if (this->re.io.DISP3DCNT.poly_vtx_ram_overflow) {
        printf("\nVTX OVERFLOW...");
        return;
    }

    // Add to vertex cache
    struct NDS_GE_VTX_list_node *o = vertex_list_add_to_end(&VTX_LIST, 0);
    transform_vertex_on_ingestion(this, o);

    switch(this->ge.params.vtx_strip.mode) {
        case NDS_GEM_SEPERATE_TRIANGLES:
            if (VTX_LIST.len >= 3) {
                printfcd("\nDO POLY SEPARATE TRI");
                this->ge.winding_order = 0;
                ingest_poly(this, CCW);
                // clear the cache
                vertex_list_init(&VTX_LIST);
            }
            break;
        case NDS_GEM_SEPERATE_QUADS:
            if (VTX_LIST.len >= 4) {
                printfcd("\nDO POLY SEPARATE QUAD");
                this->ge.winding_order = 0;
                ingest_poly(this, CCW);
                vertex_list_init(&VTX_LIST);
            }
            break;
        case NDS_GEM_TRIANGLE_STRIP:
            if (VTX_LIST.len >= 3) {
                ingest_poly(this, this->ge.winding_order);
                this->ge.winding_order ^= 1;
                vertex_list_delete_first(&VTX_LIST);
            }
            break;
        case NDS_GEM_QUAD_STRIP:
            if (VTX_LIST.len >= 4) {
                this->ge.winding_order = 0;
                ingest_poly(this, this->ge.winding_order);
                vertex_list_delete_first(&VTX_LIST);
                vertex_list_delete_first(&VTX_LIST);
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
    this->ge.params.vtx.x = (i16)(DATA[0] & 0xFFFF);
    this->ge.params.vtx.y = (i16)(DATA[0] >> 16);
    this->ge.params.vtx.z = (i16)(DATA[1] & 0xFFFF);
    ingest_vertex(this);
    //i32 x = DATA[0] &
}

static void cmd_VTX_DIFF(struct NDS *this)
{
    i16 xd = (i16)(DATA[0] & 0x3FF);
    i16 yd = (i16)((DATA[0] >> 10) & 0x3FF);
    i16 zd = (i16)((DATA[0] >> 20) & 0x3FF);
    xd <<= 6;
    yd <<= 6;
    zd <<= 6;
    xd >>= 6;
    yd >>= 6;
    zd >>= 6;
    this->ge.params.vtx.x += xd;
    this->ge.params.vtx.y += yd;
    this->ge.params.vtx.z += zd;

    ingest_vertex(this);
}

static void cmd_VTX_XY(struct NDS *this)
{
    this->ge.params.vtx.x = (i16)(DATA[0] & 0xFFFF);
    this->ge.params.vtx.y = (i16)(DATA[0] >> 16);
    ingest_vertex(this);
}

static void cmd_VTX_XZ(struct NDS *this)
{
    this->ge.params.vtx.x = (i16)(DATA[0] & 0xFFFF);
    this->ge.params.vtx.z = (i16)(DATA[0] >> 16);
    ingest_vertex(this);
}

static void cmd_VTX_YZ(struct NDS *this)
{
    this->ge.params.vtx.y = (i16)(DATA[0] & 0xFFFF);
    this->ge.params.vtx.z = (i16)(DATA[0] >> 16);
    ingest_vertex(this);
}

static void cmd_VTX_10(struct NDS *this)
{
    this->ge.params.vtx.x = (i16)((DATA[0] & 0x3FF) << 6);
    this->ge.params.vtx.y = (i16)(((DATA[0] >> 10) & 0x3FF) << 6);
    this->ge.params.vtx.z = (i16)(((DATA[0] >> 20) & 0x3FF) << 6);
    ingest_vertex(this);
}

static void cmd_SWAP_BUFFERS(struct NDS *this)
{
    printfcd("\n\n---------------------------\ncmd_SWAP_BUFFERS()");
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
    vertex_list_init(&VTX_LIST);
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
    this->re.io.viewport.height = 1 + y1 - y0;
}

static void cmd_TEXIMAGE_PARAM(struct NDS *this)
{
    printfcd("\nTEXIMAGE_PARAM");
    this->ge.params.poly.current.tex_param.u = DATA[0];
}

static void cmd_BEGIN_VTXS(struct NDS *this)
{
    // Empty vertex cache, terminate strip...
    terminate_poly_strip(this);

    // Latch parameters. TODO: did I do them all?
    this->ge.params.poly.current.attr.u = this->ge.params.poly.on_vtx_start.attr.u;
    this->ge.params.vtx_strip.mode = DATA[0] & 3;
    printfcd("\nBEGIN_VTXS(%d);", this->ge.params.poly.current.attr.mode);
}

static void cmd_PLTT_BASE(struct NDS *this)
{
    this->ge.params.poly.current.pltt_base = DATA[0] & 0x1FFF;
}

static void cmd_LIGHT_VECTOR(struct NDS *this)
{
    u32 l = DATA[0] >> 30;
    i16 dir[3];
    dir[0] = (i16)((DATA[0] & 0x000003FF) << 6) >> 6;
    dir[1] = (i16)((DATA[0] & 0x000FFC00) >> 4) >> 6;
    dir[2] = (i16)((DATA[0] & 0x3FF00000) >> 14) >> 6;
    this->ge.lights.light[l].direction[0] = (-((dir[0] * M_VECTOR[0] + dir[1] * M_VECTOR[4] + dir[2] * M_VECTOR[8] ) >> 12) << 21) >> 21;
    this->ge.lights.light[l].direction[1] = (-((dir[0] * M_VECTOR[1] + dir[1] * M_VECTOR[5] + dir[2] * M_VECTOR[9] ) >> 12) << 21) >> 21;
    this->ge.lights.light[l].direction[2] = (-((dir[0] * M_VECTOR[2] + dir[1] * M_VECTOR[6] + dir[2] * M_VECTOR[10]) >> 12) << 21) >> 21;
    i32 den = -(((dir[0] * M_VECTOR[2] + dir[1] * M_VECTOR[6] + dir[2] * M_VECTOR[10]) << 9) >> 21) + (1 << 9);

    if (den == 0) this->ge.lights.material_color.reciprocal[l] = 0;
    else this->ge.lights.material_color.reciprocal[l] = (1 << 18) / den;

}


static void C15to15(u32 c, u32 *o)
{
    o[0] = (c & 0x1F);
    o[1] = ((c >> 5) & 0x1F);
    o[2] = ((c >> 10) & 0x1F);
}

static void cmd_DIF_AMB(struct NDS *this)
{
    C15to15(DATA[0] & 0x7FFF, this->ge.lights.material_color.diffuse);
    C15to15((DATA[0] >> 16) & 0x7FFF, this->ge.lights.material_color.ambient);
    if (DATA[0] & 0x8000) {
        this->ge.params.vtx.color[0] = this->ge.lights.material_color.diffuse[0];
        this->ge.params.vtx.color[1] = this->ge.lights.material_color.diffuse[1];
        this->ge.params.vtx.color[2] = this->ge.lights.material_color.diffuse[2];
    }
}

static void cmd_SPE_EMI(struct NDS *this)
{
    C15to15(DATA[0] & 0x7FFF, this->ge.lights.material_color.reflection);
    C15to15((DATA[0] >> 16) & 0x7FFF, this->ge.lights.material_color.emission);
    this->ge.lights.shininess_enable = (DATA[0] >> 15) & 1;

}

static void cmd_LIGHT_COLOR(struct NDS *this) {
    u32 light_num = DATA[0] >> 30;
    C15to15(DATA[0] & 0x7FFF, this->ge.lights.light[light_num].color);
}

static void do_cmd(void *ptr, u64 cmd, u64 current_clock, u32 jitter)
{
    struct NDS *this = (struct NDS *)ptr;
    // Handle command!
    printfcd("\ndo_cmd %02llx", cmd);
    switch(cmd) {
#define dcmd(label) case NDS_GE_CMD_##label: cmd_##label(this); break
        dcmd(MTX_MODE);
        dcmd(END_VTXS);
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
        dcmd(SHININESS);
        dcmd(PLTT_BASE);
        dcmd(LIGHT_VECTOR);
        dcmd(DIF_AMB);
        dcmd(SPE_EMI);
        dcmd(LIGHT_COLOR);
        dcmd(VTX_DIFF);
        dcmd(VTX_XY);
        dcmd(VTX_XZ);
        dcmd(VTX_10);
        dcmd(VTX_YZ);
#undef dcmd
        default:
            printf("\nUnhandled GE cmd %02llx", cmd);
            break;
    }

    // Schedule next command if applicable!
    ge_handle_cmd(this);
}

static u32 fifo_pop_full_cmd(struct NDS *this)
{
    if (!FIFO.total_complete_cmds) {
        printfifo("\nFIFO POP FAIL NO CMDS!");
        return 0;
    }
    FIFO.total_complete_cmds--;
    struct NDS_GE_FIFO_entry *e = &FIFO.items[FIFO.head];
    u32 cmd = e->cmd;
    u32 num_params = tbl_num_params[cmd];
    printfifo("\nPOP THE HEAD & FIRST PARAM (CMD:%02x) FROM HEAD %d", cmd, FIFO.head);
    FIFO.head = (FIFO.head + 1) & 511;
    FIFO.len--;
    DATA[0] = e->data;
    u32 dpos = 1;
    for (u32 i = 1; i < num_params; i++) {
        printfifo("\nPOP A PARAMETER...");
        assert(FIFO.len>0);
        e = &FIFO.items[FIFO.head];
        DATA[dpos++] = e->data;
        FIFO.head = (FIFO.head + 1) & 511;
        FIFO.len--;
    }
    printfifo("\nFIFO POP RETURN %02x", cmd);
    return cmd;
}

static void ge_handle_cmd(struct NDS *this)
{
    // Pop a command!
    // Schedule it to be completed in N cycles!
    printfifo("\nGE_HANDLE_CMD!");
    u32 cmd;
    // Drain the FIFO looking for a good command!
    if (this->ge.io.swap_buffers) {
        printfifo("\nWAITING ON SWAP_BUFFERS");
        return;
    }
    GXSTAT.ge_busy = 0;
    if (FIFO.total_complete_cmds == 0) {
        printfifo("\nGE_HANDLE_CMD: NO COMPLETE CMDS!");
        return;
    }

    cmd = fifo_pop_full_cmd(this);
    if (!cmd) {
        printfifo("\nGE_HANDLE_CMD: FIFO POP FULL FAILED!?");
        return;
    }
    printfifo("\nGE_HANDLE_CMD: HANDLE CMD %02x", cmd);
    GXSTAT.ge_busy = 1;
    i32 num_cycles = get_num_cycles(this, cmd);
    if (cmd == NDS_GE_CMD_SWAP_BUFFERS) {
        num_cycles = 1;
    }

    fifo_update_len(this, 0, 0);
    assert(FIFO.cmd_scheduled == 0);
    printfifo("\nSCHEDULING GE CMD %02x FOR %d CYCLES!", cmd, num_cycles);
    scheduler_only_add_abs(&this->scheduler, NDS_clock_current7(this) + num_cycles, cmd, this, &do_cmd, &FIFO.cmd_scheduled);
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

static void write_fog_table(struct NDS *this, u32 addr, u32 sz, u32 val)
{
    if (sz >= 2) {
        write_fog_table(this, addr, 1, val & 0xFF);
        write_fog_table(this, addr+1, 1, (val >> 8) & 0xFF);
    }
    if (sz == 4) {
        write_fog_table(this, addr+2, 1, (val >> 16) & 0xFF);
        write_fog_table(this, addr+3, 1, (val >> 24) & 0xFF);
    }
    if (sz >= 2) return;
    this->re.io.FOG.TABLE[addr] = val;
}

static void write_edge_table(struct NDS *this, u32 addr, u32 sz, u32 val)
{
    if (sz == 4) {
        write_edge_table(this, addr, 1, val & 0xFFFF);
        write_edge_table(this, addr+2, 1, val >> 16);
        return;
    }
    if (sz >= 2) return;
    this->re.io.EDGE_TABLE_r[addr] = (val & 0x1F) << 1;
    this->re.io.EDGE_TABLE_g[addr] = ((val >> 5) & 0x1F) << 1;
    this->re.io.EDGE_TABLE_b[addr] = ((val >> 10) & 0x1F) << 1;

}


static void write_toon_table(struct NDS *this, u32 addr, u32 sz, u32 val)
{
    if (sz == 4) {
        write_toon_table(this, addr, 1, val & 0xFFFF);
        write_toon_table(this, addr+2, 1, val >> 16);
        return;
    }
    if (sz >= 2) return;
    this->re.io.TOON_TABLE_r[addr] = (val & 0x1F) << 1;
    this->re.io.TOON_TABLE_g[addr] = ((val >> 5) & 0x1F) << 1;
    this->re.io.TOON_TABLE_b[addr] = ((val >> 10) & 0x1F) << 1;
}

static void fifo_push(struct NDS *this, u32 cmd, u32 val, u32 from_dma)
{
    printfifo("\nFIFO PUSH cmd:%02x val:%08x", cmd, val);
    struct NDS_GE_FIFO_entry *e = &FIFO.items[FIFO.tail];
    FIFO.tail = (FIFO.tail + 1) & 511;
    FIFO.len++;

    e->cmd = cmd;
    e->data = val;
}

static void fifo_parse_cmd(struct NDS *this, u32 val, u32 from_dma) {
    // parse 0-3 commands and how many args each has
    // push them to cmd_queue
    FIFO.cmd_queue.len = 0;
    FIFO.cmd_queue.head = 0;
    for (u32 i = 0; i < 4; i++) {
        if (val & 0xFF) {
            FIFO.cmd_queue.items[FIFO.cmd_queue.len++].cmd = val & 0xFF;
        }
        val >>= 8;
    }

    if (FIFO.cmd_queue.len == 0) {
        printfifo("\nWAITING FOR CMD=1 (cmd was NOPs)");
        FIFO.waiting_for_cmd = 1;
        return;
    }

    for (u32 i = 0; i < FIFO.cmd_queue.len; i++) {
        u32 cmd = FIFO.cmd_queue.items[i].cmd;
        assert(tbl_cmd_good[cmd]);
        FIFO.cmd_queue.items[i].num_params_left = tbl_num_params[cmd];
        printfifo("\nCMD QUEUE %d cmd:%02x params:%d", i, cmd, FIFO.cmd_queue.items[i].num_params_left);
    }

    while(FIFO.cmd_queue.items[FIFO.cmd_queue.head].num_params_left == 0) {
        printfifo("\nCMD WITH 0 PARAMS EARLY PUSH....");
        fifo_push(this, FIFO.cmd_queue.items[FIFO.cmd_queue.head].cmd, 0, from_dma);
        FIFO.cmd_queue.len--;
        FIFO.cmd_queue.head++;
        FIFO.total_complete_cmds++;
        if (!FIFO.cmd_queue.len) break;
    }

    if (FIFO.cmd_queue.len == 0) {
        printfifo("\nWAITING FOR CMD=0 (all cmds pushed to FIFO)");
        FIFO.waiting_for_cmd = 1;
        return;
    }

    printfifo("\nWAITING FOR CMD=0 (waiting for %d params)", FIFO.cmd_queue.items[FIFO.cmd_queue.head].num_params_left);
    FIFO.waiting_for_cmd = 0;

    FIFO.cur_cmd = FIFO.cmd_queue.items[FIFO.cmd_queue.head].cmd;
}

static void fifo_drain_cmd_queue(struct NDS *this, u32 from_dma)
{
    while(FIFO.cmd_queue.items[FIFO.cmd_queue.head].num_params_left == 0) {
        FIFO.cmd_queue.head++;
        FIFO.cmd_queue.len--;
        FIFO.total_complete_cmds++;
        if (FIFO.cmd_queue.len > 0) {// If there are commands left...
            FIFO.cur_cmd = FIFO.cmd_queue.items[FIFO.cmd_queue.head].cmd;
            if (FIFO.cmd_queue.items[FIFO.cmd_queue.head].num_params_left == 0) {
                fifo_push(this, FIFO.cur_cmd, 0, from_dma);
            }
        }
        else {
            // All parameters have been passed...
            printfifo("\nWAITING FOR CMD = 1 (all params passed, num cmds: %d)", FIFO.total_complete_cmds);
            FIFO.waiting_for_cmd = 1;
            return;
        }
    }
}

static void fifo_write_direct(struct NDS *this, u32 val, u32 from_dma, u32 from_addr)
{
    if (FIFO.waiting_for_cmd) {
        printfifo("\nPARSE CMD %02x", val);
        fifo_parse_cmd(this, val, 0);
    }
    else {
        // add parameters!
        // if parameter # = 0, destroy and move to next cmd!
        // if no cmd available, set wait for cmd to 1!
        fifo_push(this, FIFO.cur_cmd, val, from_dma);
        FIFO.cmd_queue.items[FIFO.cmd_queue.head].num_params_left--;
        printfifo("\nPUSH PARAM %08x. NUM_LEFT: %d. FIFO_LEN:%d", val, FIFO.cmd_queue.items[FIFO.cmd_queue.head].num_params_left, FIFO.len);
        fifo_drain_cmd_queue(this, from_dma);
    }

    fifo_update_len(this, from_dma, 1);
    if (!from_addr && !GXSTAT.ge_busy) ge_handle_cmd(this);
}

static void fifo_write_from_addr(struct NDS *this, enum NDS_GE_cmds cmd, u32 val)
{
    printfifo("\nADDR_CMD cmd:%02x val:%08x waiting:%d", cmd, val, FIFO.waiting_for_cmd);
    if (FIFO.waiting_for_cmd) fifo_write_direct(this, cmd, 0, 1);
    fifo_write_direct(this, val, 0, 1);
    if (!GXSTAT.ge_busy) ge_handle_cmd(this);
}

void NDS_GE_write(struct NDS *this, u32 addr, u32 sz, u32 val)
{
    if ((addr >= R9_GXFIFO) && (addr < (R9_GXFIFO + 0x40))) {
        //printf("\nAddr %08x val %08x", addr, val);
        fifo_write_direct(this, val, 0, 0);
        return;
    }
    switch(addr) {
        case R9_ALPHA_TEST_REF:
            this->re.io.ALPHA_TEST_REF = val & 0xFF;
            return;
        case R9_CLRIMG_OFFSET:
            this->re.io.CLRIMAGE_OFFSET = val & 0xFFFF;
            return;
        case R9_FOG_OFFSET:
            this->re.io.FOG.OFFSET = val & 0xFFFF;
            return;
        case R9_FOG_COLOR:
            this->re.io.FOG.COLOR_r = (val & 0x1F) << 1;
            this->re.io.FOG.COLOR_g = ((val >> 5) & 0x1F) << 1;
            this->re.io.FOG.COLOR_b = ((val >> 10) & 0x1F) << 1;
        case R9_CLEAR_COLOR: {
            this->re.io.CLEAR.COLOR = val & 0x7FFF;
            this->re.io.CLEAR.fog_to_rear_plane = (val >> 15) & 1;
            this->re.io.CLEAR.alpha = (val >> 16) & 0x1F;
            this->re.io.CLEAR.poly_id = (val >> 24) & 0x3F;
            return; }
        case R9_CLEAR_DEPTH:
            assert(sz==2);
            val &= 0x7FFF;
            // The 15bit Depth is expanded to 24bit as "X=(X*200h)+((X+1)/8000h)*1FFh".
            this->re.io.CLEAR.depth = (val * 0x200) + ((val + 1) / 0x8000) * 0x1FF;
            this->re.io.CLEAR.depth = SIGNe24to32(this->re.io.CLEAR.depth);
            this->re.io.CLEAR.depth = ((u32)this->re.io.CLEAR.depth) >> 8;
            //printf("\nCLEAR VALUE SET %08x %f", this->re.io.clear.depth, vtx_to_float(this->re.io.clear.depth));
            return;
        case R9_GXSTAT:
            assert(sz==4);
            if (val & 0x8000) {
                GXSTAT.matrix_stack_over_or_underflow_error = 0;
                GXSTAT.projection_matrix_stack_level = 0;
                this->ge.matrices.stacks.position_vector_ptr = 0;
                this->ge.matrices.stacks.texture_ptr = 0;
            }
            GXSTAT.cmd_fifo_irq_mode = (val >> 30) & 3;
            if (NDS_GE_check_irq(this))
                NDS_update_IF9(this, NDS_IRQ_GXFIFO);
            return;


#define gcmd(label) case R9_G_CMD_##label: fifo_write_from_addr(this, NDS_GE_CMD_##label, val); return
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
        gcmd(DIF_AMB);
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
    if ((addr >= R9_EDGE_TABLE) && (addr < (R9_EDGE_TABLE + 0x10))) {
        addr = (addr - R9_EDGE_TABLE) >> 1;
        assert(addr<8);
        write_edge_table(this, addr, sz, val);
        return;
    }
    if ((addr >= R9_FOG_TABLE) && (addr < (R9_FOG_TABLE + 0x20))) {
        addr = addr - R9_FOG_TABLE;
        assert(addr<32);
        write_fog_table(this, addr, sz, val);
        return;
    }
    if ((addr >= R9_TOON_TABLE) && (addr < (R9_TOON_TABLE + 0x40))) {
        addr = (addr - R9_TOON_TABLE) >> 1;
        assert(addr<32);
        write_toon_table(this, addr, sz, val);
        return;
    }

    printf("\nUnhandled write to GE addr:%08x sz:%d val:%08x", addr, sz, val);
}

static u32 read_results(struct NDS *this, u32 addr, u32 sz)
{
    if ((addr >= 0x04000630) && (addr < 0x04000638)) {
        printf("\nVEC TEST READ");
        u32 which = (addr - 0x04000630) >> 2;
        assert(which<2);
        return this->ge.results.vector[which];
    }
    if ((addr >= 0x04000640) && (addr < 0x04000680)) {
        printf("\nCLIPMATRIX READ");
        if (this->ge.clip_mtx_dirty) calculate_clip_matrix(this);
        return this->ge.matrices.clip[(addr & 0x3F) >> 2];
    }
    printf("\nUNHANDLED READ FROM RESULTS AT %08x", addr);
    return 0;
}

u32 NDS_GE_read(struct NDS *this, u32 addr, u32 sz)
{
    u32 v;
    switch(addr) {
        case R9_G_CMD_POLYGON_ATTR:
            return 0;
        case R9_GXSTAT:
            assert(sz==4);
            return GXSTAT.u;
        case 0x04000620: printf("\nPOS TEST READ"); return this->ge.results.pos_test[0];
        case 0x04000624: printf("\nPOS TEST READ"); return this->ge.results.pos_test[1];
        case 0x04000628: printf("\nPOS TEST READ"); return this->ge.results.pos_test[2];
        case 0x0400062C: printf("\nPOS TEST READ"); return this->ge.results.pos_test[3];

        case 0x04000680: return M_VECTOR[0];
        case 0x04000684: return M_VECTOR[1];
        case 0x04000688: return M_VECTOR[2];
        case 0x0400068C: return M_VECTOR[4];
        case 0x04000690: return M_VECTOR[5];
        case 0x04000694: return M_VECTOR[6];
        case 0x04000698: return M_VECTOR[8];
        case 0x0400069C: return M_VECTOR[9];
        case 0x040006A0: return M_VECTOR[10];
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
    printfifo("\n\ndo_swap_buffers()!!");
    struct NDS *this = (struct NDS *)ptr;
    this->ge.io.swap_buffers = 0;
    this->ge.ge_has_buffer ^= 1;
    this->ge.buffers[this->ge.ge_has_buffer].polygon_index = 0;
    this->ge.buffers[this->ge.ge_has_buffer].vertex_index = 0;
    this->re.io.DISP3DCNT.poly_vtx_ram_overflow = 0;
    NDS_RE_render_frame(this);
    ge_handle_cmd(this);
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
