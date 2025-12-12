//
// Created by . on 3/9/25.
//
#include <cassert>
#include <cstdlib>
#include <cstring>
#include "nds_ge.h"
#include "system/nds/nds_bus.h"
#include "system/nds/nds_irq.h"
#include "system/nds/nds_regs.h"
#include "system/nds/nds_dma.h"
#include "nds_3dmath.h"
#include "helpers/multisize_memaccess.cpp"

#define printfifo(...) (void)0
//#define printffifo(...) printf(__VA_ARGS__)
#define printfcd(...) (void)0
//#define printfcd(...) printf(__VA_ARGS__)

namespace NDS::GFX {
static inline u32 c15to18(u32 color)
{
    u32 r = ((color << 1) & 0x3E);
    u32 g = ((color >> 4) & 0x3E);
    u32 b = ((color >> 9) & 0x3E);
    /*r |= (r >> 5);
    g |= (g >> 5);
    b |= (b >> 5);*/
    return r | (g << 6) | (b << 12);
}

static u32 tbl_num_params[0xFF];
static i32 tbl_num_cycles[0xFF];
static u32 tbl_cmd_good[0xFF];
static u32 tbl_test_busy[0xFF];

static float uv_to_float(i32 v)
{
    return ((float)v) / 16.0f;
}


static float vtx_to_float(i32 v)
{
    return static_cast<float>(v) / 4096.0f;
}

static void pprint_matrix(const char *n, i32 *s) {
    printfcd("\nMatrix %s", n);
    for (u32 y = 0; y < 16; y+=4) {
        printfcd("\n%f %f %f %f", vtx_to_float(s[y+0]), vtx_to_float(s[y+1]), vtx_to_float((s[y+2])), vtx_to_float(s[y+3]));
    }
}

#define DATA cur_cmd.data

#define VTX_LIST params.vtx.input_list
#define MS_POSITION_VECTOR_PTR matrices.stacks.position_vector_ptr
#define MS_TEXTURE_PTR matrices.stacks.texture_ptr
#define MS_PROJECTION_PTR matrices.stacks.projection_ptr
#define MS_POSITION matrices.stacks.position
#define MS_TEXTURE matrices.stacks.texture
#define MS_PROJECTION matrices.stacks.projection
#define MS_VECTOR matrices.stacks.vector
#define M_POSITION matrices.position
#define M_TEXTURE matrices.texture
#define M_PROJECTION matrices.projection
#define M_VECTOR matrices.vector
#define M_CLIP matrices.clip
#define M_SZ sizeof(MATRIX)
#define GXSTAT io.GXSTAT

static void identity_matrix(i32 *m)
{
    memset(m, 0, sizeof(i32)*16);
    m[0] = m[5] = m[10] = m[15] = 1 << 12;
}


void VTX_list::init()
{
    pool_bitmap = 0;
    len = 0;
    first = nullptr;
    last = nullptr;
}

static VTX_list_node *vertex_list_alloc_node(VTX_list *l, u32 vtx_parent)
{
    if (l->len >= NDS_GE_VTX_LIST_MAX) {
        NOGOHERE;
        return nullptr;
    }
    u32 poolbit = 1;
    u32 poolentry = 0;
    for (u32 i = 0; i < NDS_GE_VTX_LIST_MAX; i++) {
        if (!(l->pool_bitmap & poolbit)) break;
        poolentry += 1;
        poolbit <<= 1;
    }
    l->pool_bitmap |= poolbit;

    VTX_list_node *a = &l->pool[poolentry];
    a->poolclear = ~poolbit;
    a->data.processed = 0;
    a->data.vtx_parent = vtx_parent;
    a->next = a->prev = nullptr;
    return a;
}

static void vertex_list_free_node(VTX_list *p, VTX_list_node *n)
{
    p->pool_bitmap &= n->poolclear;
    n->next = n->prev = nullptr;
}

static void vertex_list_swap_last_two(VTX_list *l)
{
    VTX_list_node *next_to_last = l->last->prev;
    VTX_list_node *last = l->last;
    assert(l->first!=l->last);
    assert(l->first!=next_to_last);
    assert(l->last->next == nullptr);
    next_to_last->prev->next = last;
    last->prev = next_to_last->prev;
    last->next = next_to_last;
    next_to_last->prev = last;
    next_to_last->next = nullptr;
    l->last = next_to_last;
}

static void vertex_list_delete_first(VTX_list *l)
{
    if (l->len == 0) return;
    if (l->len == 1) {
        l->first = l->last = nullptr;
        l->pool_bitmap = 0;
        l->len = 0;
        return;
    }
    if (l->len == 2) {
        vertex_list_free_node(l, l->first);
        l->first = l->last;
        l->last->prev = nullptr;
        l->len--;
        return;
    }
    VTX_list_node *next = l->first->next;
    next->prev = nullptr;
    vertex_list_free_node(l, l->first);
    l->first = next;
    l->len--;
}


static VTX_list_node *vertex_list_add_to_end(VTX_list *l, u32 vtx_parent)
{
    VTX_list_node *n = vertex_list_alloc_node(l, vtx_parent);
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

static void copy_vertex_list_into(VTX_list *dest, VTX_list *src)
{
    dest->init();
    VTX_list_node *src_node = src->first;
    i32 num = 0;
    while(src_node) {
        VTX_list_node *dst_node = vertex_list_add_to_end(dest, num++);
        memcpy(&dst_node->data, &src_node->data, sizeof(VTX_list_node_data));
        src_node = src_node->next;
    }
}

GE::GE(NDS::core *parent, scheduler_t *scheduler_in) : bus(parent), scheduler(scheduler_in) {
    ge_has_buffer = 0;
    for (u32 i = 0; i < 0xFF; i++) {
        tbl_num_params[i] = 0;
        tbl_num_cycles[i] = 0;
        tbl_cmd_good[i] = 0;
        tbl_test_busy[i] = 0;
    }

    tbl_test_busy[CMD_BOX_TEST] = 1;
    tbl_test_busy[CMD_VEC_TEST] = 1;
    tbl_test_busy[CMD_POS_TEST] = 1;

#define SCMD(cmd_id, np, ncyc) tbl_num_params[CMD_##cmd_id] = np; tbl_num_cycles[CMD_##cmd_id] = ncyc; tbl_cmd_good[CMD_##cmd_id] = 1
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

    reset();
}

void GE::reset()
{
    // Load identity matrices everywhere
    identity_matrix(M_PROJECTION);
    identity_matrix(M_CLIP);
    identity_matrix(M_POSITION);
    identity_matrix(M_TEXTURE);
    identity_matrix(M_VECTOR);
    VTX_LIST.init();
    fifo.head = fifo.tail = fifo.len = 0;
    fifo.pausing_cpu = 0;
    printfifo("\nWAITING CMD=1 (reset)");
    fifo.waiting_for_cmd = 1;
}

void FIFO::update_len(bool from_dma, bool did_add)
{
    u32 old_bit = 0;
    if (ge->GXSTAT.cmd_fifo_irq_mode) {
        old_bit = ge->check_irq();
    }

    bool old_pausing_cpu = pausing_cpu;
    pausing_cpu = len > 255;
    if (!old_pausing_cpu && pausing_cpu) {
        //printf("\nFIFO PAUSE CPU");
    }
    else if (old_pausing_cpu && !pausing_cpu) {
        //printf("\nFIFO UNPAUSE CPU!");
    }
    ge->GXSTAT.cmd_fifo_len = len > 255 ? 255 : len;
    ge->GXSTAT.cmd_fifo_less_than_half_full = len < 128;
    ge->GXSTAT.cmd_fifo_empty = len == 0;

    if (ge->GXSTAT.cmd_fifo_irq_mode) {
        bool new_bit = ge->check_irq();
        if ((old_bit != new_bit) && new_bit)
            ge->bus->update_IF9(IRQ_GXFIFO);
    }

    if (!did_add && !from_dma && (len < 128))
        ge->bus->trigger_dma9_if(DMA_GE_FIFO);
}

i32 GE::get_num_cycles(u32 cmd) const {
    i32 num_cycles = tbl_num_cycles[cmd];
    if (num_cycles == -1) { // Calculate number of cycles a bit differently
        i32 mtx_add = io.MTX_MODE == 2 ? 30 : 0;
        switch(cmd) {
            case CMD_MTX_MULT_4x4:
                return 35 + mtx_add;
            case CMD_MTX_MULT_4x3:
                return 31 + mtx_add;
            case CMD_MTX_MULT_3x3:
                return 28 + mtx_add;
            case CMD_MTX_TRANS:
                return 22 + mtx_add;
            case CMD_NORMAL:
                return 8 + params.poly.current.num_lights;
            default:
            NOGOHERE;
        }
    }
    return num_cycles;
}

void GE::cmd_END_VTXS()
{

}

void GE::cmd_MTX_MODE()
{
    io.MTX_MODE = DATA[0] & 3;
    printfcd("\nMTX_MODE(%d);", io.MTX_MODE);
}

void GE::calculate_clip_matrix()
{
    clip_mtx_dirty = false;
    // Clip matrix = projection * position
    memcpy(M_CLIP, M_PROJECTION, M_SZ);
    matrix_multiply_4x4(M_CLIP, M_POSITION);
}

void GE::cmd_MTX_PUSH()
{
    // ClipMatrix = PositionMatrix * ProjectionMatrix
    u32 old_ptr;
    switch(io.MTX_MODE) {
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

void GE::cmd_MTX_POP()
{
    u32 old_ptr;
    switch(io.MTX_MODE) {
        case 0:
            GXSTAT.matrix_stack_over_or_underflow_error |= MS_PROJECTION_PTR == 0;
            MS_PROJECTION_PTR = (MS_PROJECTION_PTR - 1) & 1;
            GXSTAT.projection_matrix_stack_level = MS_PROJECTION_PTR;
            memcpy(&M_PROJECTION, &MS_PROJECTION[MS_PROJECTION_PTR], M_SZ);
            clip_mtx_dirty = true;
            break;
        case 1:
        case 2: {
            i32 n = ((i32)DATA[0] << 26) >> 26;
            MS_POSITION_VECTOR_PTR = (MS_POSITION_VECTOR_PTR - n) & 63;
            GXSTAT.matrix_stack_over_or_underflow_error |= (MS_POSITION_VECTOR_PTR) > 30;
            GXSTAT.position_vector_matrix_stack_level = MS_POSITION_VECTOR_PTR;
            memcpy(&M_POSITION, &MS_POSITION[MS_POSITION_VECTOR_PTR], M_SZ);
            memcpy(&M_VECTOR, &MS_VECTOR[MS_POSITION_VECTOR_PTR], M_SZ);
            clip_mtx_dirty = true;
            break; }
        case 3:
            GXSTAT.matrix_stack_over_or_underflow_error |= MS_TEXTURE_PTR == 0;
            MS_TEXTURE_PTR = (MS_TEXTURE_PTR - 1) & 1;
            memcpy(&M_TEXTURE, &MS_TEXTURE[MS_TEXTURE_PTR], M_SZ);
            // no texture matrix GXSTAT bits
            break;
    }
}

void GE::cmd_MTX_STORE()
{
    // bit 0..4 is stack address. copy stack[S] to stack[N]
    switch(io.MTX_MODE) {
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

void GE::cmd_MTX_RESTORE()
{
    // bit 0..4 is stack address. copy stack[N] to stack[S]
    switch(io.MTX_MODE) {
        case 0:
            memcpy(&M_PROJECTION, &MS_PROJECTION[0], M_SZ);
            clip_mtx_dirty = true;
            break;
        case 1:
        case 2: {
            u32 n = DATA[0] & 0x1F;
            GXSTAT.matrix_stack_over_or_underflow_error |= n > 30;
            memcpy(&M_VECTOR, &MS_VECTOR[n], M_SZ);
            memcpy(&M_POSITION, &MS_POSITION[n], M_SZ);
            clip_mtx_dirty = true;
            break; }
        case 3:
            memcpy(&M_TEXTURE, &MS_TEXTURE[0], M_SZ);
            break;
    }
}

void GE::cmd_MTX_IDENTITY()
{
    switch(io.MTX_MODE) {
        case 0:
            identity_matrix(M_PROJECTION);
            clip_mtx_dirty = true;
            return;
        case 1:
            identity_matrix(M_POSITION);
            clip_mtx_dirty = true;
            return;
        case 2:
            identity_matrix(M_POSITION);
            identity_matrix(M_VECTOR);
            clip_mtx_dirty = true;
            return;
        case 3:
            identity_matrix(M_TEXTURE);
            return;
    }
}

void GE::cmd_MTX_LOAD_4x4()
{
    switch(io.MTX_MODE) {
        case 0:
            matrix_load_4x4(M_PROJECTION, (i32 *) DATA);
            clip_mtx_dirty = true;
            return;
        case 1:
            matrix_load_4x4(M_POSITION, (i32 *) DATA);
            clip_mtx_dirty = true;
            return;
        case 2:
            matrix_load_4x4(M_POSITION, (i32 *) DATA);
            matrix_load_4x4(M_VECTOR, (i32 *) DATA);
            clip_mtx_dirty = true;
            return;
        case 3:
            matrix_load_4x4(M_TEXTURE, (i32 *) DATA);
            return;
    }
}

void GE::cmd_MTX_LOAD_4x3()
{
    switch(io.MTX_MODE) {
        case 0:
            matrix_load_4x3(M_PROJECTION, (i32 *) DATA);
            clip_mtx_dirty = true;
            return;
        case 1:
            matrix_load_4x3(M_POSITION, (i32 *) DATA);
            clip_mtx_dirty = true;
            return;
        case 2:
            matrix_load_4x3(M_POSITION, (i32 *) DATA);
            matrix_load_4x3(M_VECTOR, (i32 *) DATA);
            clip_mtx_dirty = true;
            return;
        case 3:
            matrix_load_4x3(M_TEXTURE, (i32 *) DATA);
            return;
    }
}

void GE::cmd_SHININESS() {
    u32 i = 0;
    for (u32 p = 0; p < 32; p++) {
        u32 word = DATA[p];
        for (u32 w = 0; w < 4; w++) {
            re->io.SHININESS[i++] = word & 0xFF;
            word >>= 8;
        }
    }
};

void GE::cmd_MTX_MULT_4x4()
{
    switch(io.MTX_MODE) {
        case 0: // projection
            matrix_multiply_4x4(M_PROJECTION, (i32 *)DATA);
            clip_mtx_dirty = true;
            return;
        case 1: //  pos/coord matrix
            matrix_multiply_4x4(M_POSITION, (i32 *)DATA);
            clip_mtx_dirty = true;
            return;
        case 2: // both pos/coord and dir/vector matrices
            matrix_multiply_4x4(M_POSITION, (i32 *)DATA);
            matrix_multiply_4x4(M_VECTOR, (i32 *)DATA);
            clip_mtx_dirty = true;
            return;
        case 3: // texture matrix
            matrix_multiply_4x4(M_TEXTURE, (i32 *)DATA);
            return;
    }
}

void GE::cmd_MTX_MULT_4x3()
{
    switch(io.MTX_MODE) {
        case 0: // projection
            matrix_multiply_4x3(M_PROJECTION, (i32 *)DATA);
            clip_mtx_dirty = true;
            return;
        case 1: //  pos/coord matrix
            matrix_multiply_4x3(M_POSITION, (i32 *)DATA);
            clip_mtx_dirty = true;
            return;
        case 2: // both pos/coord and dir/vector matrices
            matrix_multiply_4x3(M_POSITION, (i32 *)DATA);
            matrix_multiply_4x3(M_VECTOR, (i32 *)DATA);
            clip_mtx_dirty = true;
            return;
        case 3: // texture matrix
            matrix_multiply_4x3(M_TEXTURE, (i32 *)DATA);
            printfcd("\nMTX_MULT_4x3(texture);");
            return;
    }
}

void GE::cmd_MTX_MULT_3x3()
{
    switch(io.MTX_MODE) {
        case 0: // projection
            matrix_multiply_3x3(M_PROJECTION, (i32 *)DATA);
            clip_mtx_dirty = true;
            return;
        case 1: //  pos/coord matrix
            matrix_multiply_3x3(M_POSITION, (i32 *)DATA);
            clip_mtx_dirty = true;
            return;
        case 2: // both pos/coord and dir/vector matrices
            matrix_multiply_3x3(M_POSITION, (i32 *)DATA);
            matrix_multiply_3x3(M_VECTOR, (i32 *)DATA);
            clip_mtx_dirty = true;
            return;
        case 3: // texture matrix
            matrix_multiply_3x3(M_TEXTURE, (i32 *)DATA);
            return;
    }
}

void GE::cmd_MTX_SCALE()
{
    switch(io.MTX_MODE) {
        case 0:
            matrix_scale(M_PROJECTION, (i32 *)DATA);
            clip_mtx_dirty = true;
            return;
        case 1:
        case 2:
            matrix_scale(M_POSITION, (i32 *)DATA);
            clip_mtx_dirty = true;
            return;
        case 3:
            matrix_scale(M_TEXTURE, (i32 *)DATA);
            return;
    }
}

void GE::cmd_MTX_TRANS()
{
    switch(io.MTX_MODE) {
        case 0:
            matrix_translate(M_PROJECTION, (i32 *)DATA);
            clip_mtx_dirty = true;
            return;
        case 1:
            matrix_translate(M_POSITION, (i32 *)DATA);
            clip_mtx_dirty = true;
            return;
        case 2:
            matrix_translate(M_POSITION, (i32 *)DATA);
            matrix_translate(M_VECTOR, (i32 *)DATA);
            clip_mtx_dirty = true;
            return;
        case 3:
            matrix_translate(M_TEXTURE, (i32 *)DATA);
            return;
    }
}

void GE::cmd_COLOR()
{
    u32 color_in = DATA[0];
    u32 r = (color_in >> 0) & 0x1F;
    u32 g = (color_in >> 5) & 0x1F;
    u32 b = (color_in >> 10) & 0x1F;
    params.vtx.color[0] = r;
    params.vtx.color[1] = g;
    params.vtx.color[2] = b;
}

void GE::cmd_NORMAL()
{
    lights.normal[0] = (i16)((DATA[0] & 0x000003FF) << 6) >> 6;
    lights.normal[1] = (i16)((DATA[0] & 0x000FFC00) >> 4) >> 6;
    lights.normal[2] = (i16)((DATA[0] & 0x3FF00000) >> 14) >> 6;

    if (params.poly.current.tex_param.texture_coord_transform_mode ==  NDS_TCTM_normal) {
        params.vtx.uv[0] = params.vtx.original_uv[0] + (((i64) lights.normal[0]*M_TEXTURE[0] + (i64) lights.normal[1]*M_TEXTURE[4] + (i64) lights.normal[2]*M_TEXTURE[8]) >> 21);
        params.vtx.uv[1] = params.vtx.original_uv[1] + (((i64) lights.normal[0]*M_TEXTURE[1] + (i64) lights.normal[1]*M_TEXTURE[5] + (i64) lights.normal[2]*M_TEXTURE[9]) >> 21);
    }

    i32 normaltrans[3]; // should be 1 bit sign 10 bits frac
    normaltrans[0] = ((lights.normal[0]*M_VECTOR[0] + lights.normal[1]*M_VECTOR[4] + lights.normal[2]*M_VECTOR[8]) << 9) >> 21;
    normaltrans[1] = ((lights.normal[0]*M_VECTOR[1] + lights.normal[1]*M_VECTOR[5] + lights.normal[2]*M_VECTOR[9]) << 9) >> 21;
    normaltrans[2] = ((lights.normal[0]*M_VECTOR[2] + lights.normal[1]*M_VECTOR[6] + lights.normal[2]*M_VECTOR[10]) << 9) >> 21;

    i32 c = 0;
    u32 vtxbuff[3] = {(u32)lights.material_color.emission[0] << 14, (u32)lights.material_color.emission[1] << 14, (u32)lights.material_color.emission[2] << 14 };
    for (int i = 0; i < 4; i++)
    {
        if (!(params.poly.current.attr.u & (1<<i)))
            continue;

        i32 dot = ((lights.light[i].direction[0]*normaltrans[0]) >> 9) + ((lights.light[i].direction[1]*normaltrans[1]) >> 9) + ((lights.light[i].direction[2]*normaltrans[2]) >> 9);

        i32 shinelevel;
        if (dot > 0) {
            i32 diffdot = (dot << 21) >> 21;
            vtxbuff[0] += (lights.material_color.diffuse[0] * lights.light[i].color[0] * diffdot) & 0xFFFFF;
            vtxbuff[1] += (lights.material_color.diffuse[1] * lights.light[i].color[1] * diffdot) & 0xFFFFF;
            vtxbuff[2] += (lights.material_color.diffuse[2] * lights.light[i].color[2] * diffdot) & 0xFFFFF;

            dot += normaltrans[2];

            dot = (dot << 21) >> 21;
            dot = ((dot * dot) >> 10) & 0x3FF;

            shinelevel = ((dot * lights.material_color.reciprocal[i]) >> 8) - (1<<9);

            if (shinelevel < 0) shinelevel = 0;
            else {
                shinelevel = (shinelevel << 18) >> 18;
                if (shinelevel < 0) shinelevel = 0;
                else if (shinelevel > 0x1FF) shinelevel = 0x1FF;
            }
        }
        else shinelevel = 0;

        if (lights.shininess_enable)
        {
            shinelevel >>= 2;
            shinelevel = re->io.SHININESS[shinelevel];
            shinelevel <<= 1;
        }

        vtxbuff[0] += ((lights.material_color.reflection[0] * shinelevel) + (lights.material_color.ambient[0] << 9)) * lights.light[i].color[0];
        vtxbuff[1] += ((lights.material_color.reflection[1] * shinelevel) + (lights.material_color.ambient[1] << 9)) * lights.light[i].color[1];
        vtxbuff[2] += ((lights.material_color.reflection[2] * shinelevel) + (lights.material_color.ambient[2] << 9)) * lights.light[i].color[2];

        c++;
    }

    params.vtx.color[0] = ((vtxbuff[0] >> 14) > 31) ? 31 : (vtxbuff[0] >> 14);
    params.vtx.color[1] = ((vtxbuff[1] >> 14) > 31) ? 31 : (vtxbuff[1] >> 14);
    params.vtx.color[2] = ((vtxbuff[2] >> 14) > 31) ? 31 : (vtxbuff[2] >> 14);
}

void GE::cmd_TEXCOORD()
{
    params.vtx.original_uv[0] = DATA[0] & 0xFFFF;
    params.vtx.original_uv[1] = DATA[0] >> 16;
    if (params.poly.current.tex_param.texture_coord_transform_mode == NDS_TCTM_texcoord) {
        params.vtx.uv[0] = (params.vtx.original_uv[0]*M_TEXTURE[0] + params.vtx.original_uv[1]*M_TEXTURE[4] + M_TEXTURE[8] + M_TEXTURE[12]) >> 12;
        params.vtx.uv[1] = (params.vtx.original_uv[0]*M_TEXTURE[1] + params.vtx.original_uv[1]*M_TEXTURE[5] + M_TEXTURE[9] + M_TEXTURE[13]) >> 12;
    }
    else
    {
        params.vtx.uv[0] = params.vtx.original_uv[0];
        params.vtx.uv[1] = params.vtx.original_uv[1];
    }
}

#define CCW 0
#define CW 1

static inline u32 C5to18(u32 c) {
    return c ? ((c << 12) + 0xFFF) : 0;
}

void GE::transform_vertex_on_ingestion(VTX_list_node *node)
{
    if (clip_mtx_dirty) calculate_clip_matrix();
    node->data.color[0] = C5to18(params.vtx.color[0]);
    node->data.color[1] = C5to18(params.vtx.color[1]);
    node->data.color[2] = C5to18(params.vtx.color[2]);
    node->data.processed = 0;

    i64 tmp[4] = {static_cast<i64>(params.vtx.x), static_cast<i64>(params.vtx.y), static_cast<i64>(params.vtx.z), 1 << 12};
    node->data.xyzw[0] = static_cast<i32>((tmp[0] * M_CLIP[0] + tmp[1] * M_CLIP[4] + tmp[2] * M_CLIP[8] + tmp[3] * M_CLIP[12]) >> 12);
    node->data.xyzw[1] = static_cast<i32>((tmp[0] * M_CLIP[1] + tmp[1] * M_CLIP[5] + tmp[2] * M_CLIP[9] + tmp[3] * M_CLIP[13]) >> 12);
    node->data.xyzw[2] = static_cast<i32>((tmp[0] * M_CLIP[2] + tmp[1] * M_CLIP[6] + tmp[2] * M_CLIP[10] + tmp[3] * M_CLIP[14]) >> 12);
    node->data.xyzw[3] = static_cast<i32>((tmp[0] * M_CLIP[3] + tmp[1] * M_CLIP[7] + tmp[2] * M_CLIP[11] + tmp[3] * M_CLIP[15]) >> 12);

    if (params.poly.current.tex_param.texture_coord_transform_mode == NDS_TCTM_vertex) {
        node->data.uv[0] = ((tmp[0]*M_TEXTURE[0] + tmp[1]*M_TEXTURE[4] + tmp[2]*M_TEXTURE[8]) >> 24) + params.vtx.original_uv[0];
        node->data.uv[1] = ((tmp[0]*M_TEXTURE[1] + tmp[1]*M_TEXTURE[5] + tmp[2]*M_TEXTURE[9]) >> 24) + params.vtx.original_uv[1];
    }
    else {
        node->data.uv[0] = params.vtx.uv[0];
        node->data.uv[1] = params.vtx.uv[1];
    }
}

static void vertex_list_add_copy(VTX_list *l, VTX_list_node *src)
{
    VTX_list_node *dest = vertex_list_add_to_end(l, 0);
    memcpy(&dest->data, &src->data, sizeof(dest->data));
}

static void clip_segment(u32 comp, i32 plane, u32 attribs, VTX_list *outlist, VTX_list_node *vin, VTX_list_node *vout)
{
    i64 factor_num = vin->data.xyzw[3] - (plane*vin->data.xyzw[comp]);
    i32 factor_den = factor_num - (vout->data.xyzw[3] - (plane*vout->data.xyzw[comp]));
    VTX_list_node *outbuf = vertex_list_add_to_end(outlist, 0);

#define INTERPOLATE(val)  { outbuf->data.val = (vin->data.val + ((vout->data.val - vin->data.val) * factor_num) / factor_den); }

    if (comp != 0) INTERPOLATE(xyzw[0]);
    if (comp != 1) INTERPOLATE(xyzw[1]);
    if (comp != 2) INTERPOLATE(xyzw[2]);
    INTERPOLATE(xyzw[3]);
    outbuf->data.xyzw[comp] = plane*outbuf->data.xyzw[3];

    if (attribs)
    {
        INTERPOLATE(color[0]);
        INTERPOLATE(color[1]);
        INTERPOLATE(color[2]);

        INTERPOLATE(uv[0]);
        INTERPOLATE(uv[1]);
    }
    outbuf->data.processed = 0;

#undef INTERPOLATE
}

void GE::clip_verts_on_plane(u32 comp, u32 attribs, VTX_list *vertices)
{
    VTX_list tmp;
    tmp.init();
    VTX_list_node *vtx = vertices->first;
    VTX_list_node *vprev, *vnext;

    while(vtx) {
        if (vtx->data.xyzw[comp] > vtx->data.xyzw[3]) {
            if ((comp == 2) && (!params.poly.current.attr.render_if_intersect_far_plane)) {
                vertices->init();
                return;
            }

            vprev = vtx->prev; if (!vprev) vprev = vertices->last;
            if (vprev->data.xyzw[comp] <= vprev->data.xyzw[3])
                clip_segment(comp, 1, attribs, &tmp, vtx, vprev);

            vnext = vtx->next; if (!vnext) vnext = vertices->first;
            if (vnext->data.xyzw[comp] <= vnext->data.xyzw[3])
                clip_segment(comp, 1, attribs, &tmp, vtx, vnext);
        }
        else {
            vertex_list_add_copy(&tmp, vtx);
        }
        vtx = vtx->next;
    }

    vertices->init();
    vtx = tmp.first;

    while(vtx) {
        if (vtx->data.xyzw[comp] < -vtx->data.xyzw[3]) {
            vprev = vtx->prev; if (!vprev) vprev = tmp.last;
            if (vprev->data.xyzw[comp] >= -vprev->data.xyzw[3])
                clip_segment(comp, -1, attribs, vertices, vtx, vprev);

            vnext = vtx->next; if (!vnext) vnext = tmp.first;
            if (vnext->data.xyzw[comp] >= -vnext->data.xyzw[3])
                clip_segment(comp, -1, attribs, vertices, vtx, vnext);
        }
        else
            vertex_list_add_copy(vertices, vtx);
        vtx = vtx->next;
    }
    
    vtx = vertices->first;
    while(vtx) {
        vtx->data.color[0] &= ~0xFFF;
        vtx->data.color[0] += 0xFFF;
        vtx->data.color[1] &= ~0xFFF;
        vtx->data.color[1] += 0xFFF;
        vtx->data.color[2] &= ~0xFFF;
        vtx->data.color[2] += 0xFFF;

        vtx = vtx->next;
    }
}

void GE::clip_verts(POLY *out) {
    clip_verts_on_plane(2, true, &out->vertex_list);
    clip_verts_on_plane(1, true, &out->vertex_list);
    clip_verts_on_plane(0, true, &out->vertex_list);
}

static u32 determine_needs_clipping(VTX_list_node *v)
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

u32 GE::commit_vertex(VTX_list_node *v, i32 xx, i32 yy, i32 zz, i32 ww, i32 *uv, u32 cr, u32 cg, u32 cb)
{
    BUFFERS *b = &buffers[ge_has_buffer];
    if (ge_has_buffer > 1) {
        printf("\nWOAH! cyc:%lld", bus->clock.master_cycle_count7+bus->waitstates.current_transaction);
    }
    u32 addr = b->vertex_index;

    /*b->vertex_index++;
    if (b->vertex_index >= 6144) {
        re.io.DISP3DCNT.poly_vtx_ram_overflow = 1;
    }

    VERTEX *v = &b->vertex[addr];*/

    // We need...xx, yy, zz, ww, uv, and color
    // 32 bits each = 24 bytes...
    v->data.xyzw[0] = xx;
    v->data.xyzw[1] = yy;
    v->data.xyzw[2] = zz;
    v->data.xyzw[3] = ww;
    v->data.uv[0] = uv[0];
    v->data.uv[1] = uv[1];
    cr >>= 12;
    cg >>= 12;
    cb >>= 12;
    v->data.color[0] = cr ? ((cr << 4) + 0xF) : 0;
    v->data.color[1] = cg ? ((cg << 4) + 0xF) : 0;
    v->data.color[2] = cb ? ((cb << 4) + 0xF) : 0;

    return addr;
}

static void normalize_w(POLY *out)
{
    out->w_normalization_left = 0;
    out->w_normalization_right = 0;
    u32 longest_one = 0;
    VTX_list_node *item = out->vertex_list.first;
    u32 wsize = 0;
    while(item) {
        u32 w = static_cast<u32>(item->data.xyzw[3]);
        //if (w == 0) poly->Degenerate = true;

        while ((w >> wsize) && (wsize < 32))
            wsize += 4;
        item = item->next;
    }

    VTX_list_node *vtx = out->vertex_list.first;
    while(vtx) {
        s32 w, wshifted;

        if (wsize < 16)
        {
            w = vtx->data.xyzw[3] << (16 - wsize);
            wshifted = w >> (16 - wsize);
        }
        else
        {
            w = vtx->data.xyzw[3] >> (wsize - 16);
            wshifted = w << (wsize - 16);
        }

        i32 z;
        if (vtx->data.xyzw[3])
            z = ((((i64)vtx->data.xyzw[2] * 0x4000) / vtx->data.xyzw[3]) + 0x3FFF) * 0x200;
        else
            z = 0x7FFE00;

        if (z < 0) z = 0;
        else if (z > 0xFFFFFF) z = 0xFFFFFF;

        vtx->data.w_normalized = w;
        vtx->data.xyzw[2] = w;

        vtx = vtx->next;
    }
}

void GE::finalize_verts_and_get_first_addr(POLY *poly)
{
    // Final transform to screen and write into vertex buffer
    BUFFERS *b = &buffers[ge_has_buffer];

    //printf("\nFinalzie verts");
    // Go thru verts...
    VTX_list_node *first_node = poly->vertex_list.first;
    VTX_list_node *node = first_node;
    u32 num = 0;
    while(node) {
        if (!node->data.processed) {
            node->data.processed = 1;

            node->data.xyzw[3] &= 0x00FFFFFF;
            u32 posX, posY;
            u32 w = node->data.xyzw[3];
            if (w > 0) {
                posX = node->data.xyzw[0] + w;
                posY = -node->data.xyzw[1] + w;
                u32 den = w;

                if (w > 0xFFFF) {
                    posX >>= 1;
                    posY >>= 1;
                    den  >>= 1;
                }

                den <<= 1;
                posX = ((posX * re->io.viewport[4]) / den) + re->io.viewport[0];
                posY = ((posY * re->io.viewport[5]) / den) + re->io.viewport[3];
            }
            else {
                posX = 0;
                posY = 0;
            }

            posX = posX & 0x1FF;
            posY = posY & 0xFF;

            node->data.vram_ptr = commit_vertex(node, posX, posY, node->data.xyzw[2], node->data.xyzw[3], node->data.uv, node->data.color[0], node->data.color[1], node->data.color[2]);
        }
        node = node->next;
    }
}

static u32 edge_is_top_or_bottom(POLY *poly, VTX_list_node *v[])
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

static void determine_highest_vertex(POLY *poly, BUFFERS *b)
{
    poly->min_y = 512;
    poly->max_y = 0;
    VTX_list_node *v = poly->vertex_list.first;
    poly->highest_vertex = v;
    while(v) {
        if (v->data.xyzw[1] < poly->highest_vertex->data.xyzw[1]) poly->highest_vertex = v;
        if (v->data.xyzw[1] < poly->min_y) poly->min_y = v->data.xyzw[1];
        if (v->data.xyzw[1] > poly->max_y) poly->max_y = v->data.xyzw[1];
        v = v->next;
    }
}

static u32 determine_winding_order(POLY *poly, BUFFERS *b)
{
    // OK, now progress in order until we have a y-delta of >0.
    VTX_list_node *n = poly->highest_vertex;
    VTX_list_node *n_comp = n;
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

void GE::evaluate_edges(POLY *poly, u32 expected_winding_order)
{
    BUFFERS *b = &buffers[ge_has_buffer];
    VTX_list_node *v[2];
    v[1] = poly->vertex_list.first;
    u32 edgenum = 0;

    determine_highest_vertex(poly, b);
    poly->is_translucent = ((poly->tex_param.format == 1 || poly->tex_param.format == 6) && !(poly->attr.mode & 1)) || (poly->attr.alpha > 0 && poly->attr.alpha < 31);
    poly->is_shadow_mask = (poly->attr.poly_id== 0) && (poly->attr.mode == 3);
    poly->is_shadow = !poly->is_shadow_mask && (poly->attr.mode == 3);
    poly->sorting_key = (poly->max_y << 9) | poly->min_y | (poly->is_translucent << 24);
}

static void list_reverse(VTX_list *l)
{
    VTX_list_node *node = l->first;
    l->first= l->last;
    l->last = node;
    while(node) {
        VTX_list_node *t = node->next;

        node->next = node->prev;
        node->prev = t;

        node = t;
    }
}

void GE::ingest_poly(u32 in_winding_order) {
    BUFFERS *b = &buffers[ge_has_buffer];
    u32 addr = b->polygon_index;

    POLY *out = &b->polygon[addr];
    out->vertex_mode = params.vtx_strip.mode;
    out->sampler.filled_out = 0;
    out->attr.u = params.poly.current.attr.u;
    out->tex_param.u = params.poly.current.tex_param.u;
    out->pltt_base = params.poly.current.pltt_base;
    // TODO: add all relavent attributes to structure here

    copy_vertex_list_into(&out->vertex_list, &VTX_LIST);
    if (params.vtx_strip.mode == QUAD_STRIP) {
        vertex_list_swap_last_two(&out->vertex_list);
    }

    VTX_list_node *v0 = out->vertex_list.first;
    VTX_list_node *v1 = v0->next;
    VTX_list_node *v2 = v1->next;

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

    u32 frontface = (dot < 0) ^ in_winding_order;
    u32 backface = (dot > 0) ^ in_winding_order;

    if (frontface && (!out->attr.render_front)) {
        return;
    } else if (backface && (!out->attr.render_back)) {
        return;
    }

    // Now clip...
    u32 needs_clipping = 0;
    VTX_list_node *n = out->vertex_list.first;
    while (n) {
        needs_clipping = determine_needs_clipping(n);
        if (needs_clipping) break;
        n = n->next;
    }

    // Clip vertices!
    if (needs_clipping) {
        clip_verts(out);
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

    finalize_verts_and_get_first_addr(out);
    normalize_w(out);
    if (re->io.DISP3DCNT.poly_vtx_ram_overflow) {
        return;
    }


    evaluate_edges(out, in_winding_order);

    b->polygon_index++;
    if (b->polygon_index >= 2048) {
        re->io.DISP3DCNT.poly_vtx_ram_overflow = 1;
    }
    //printf("\npoly %d sides:%d front_facing:%d", addr, out->num_vertices, out->front_facing);
    printfcd("\nOUTPUT POLY HAS %d SIDES", out->vertex_list.len);
}

void GE::ingest_vertex() {
    if (re->io.DISP3DCNT.poly_vtx_ram_overflow) {
        //printf("\nVTX OVERFLOW...");
        return;
    }

    // Add to vertex cache
    VTX_list_node *o = vertex_list_add_to_end(&VTX_LIST, 0);
    transform_vertex_on_ingestion(o);

    switch(params.vtx_strip.mode) {
        case SEPERATE_TRIANGLES:
            if (VTX_LIST.len >= 3) {
                printfcd("\nDO POLY SEPARATE TRI");
                winding_order = 0;
                ingest_poly(CCW);
                // clear the cache
                VTX_LIST.init();
            }
            break;
        case SEPERATE_QUADS:
            if (VTX_LIST.len >= 4) {
                printfcd("\nDO POLY SEPARATE QUAD");
                winding_order = 0;
                ingest_poly(CCW);
                VTX_LIST.init();
            }
            break;
        case TRIANGLE_STRIP:
            if (VTX_LIST.len >= 3) {
                ingest_poly(winding_order);
                winding_order ^= 1;
                vertex_list_delete_first(&VTX_LIST);
            }
            break;
        case QUAD_STRIP:
            if (VTX_LIST.len >= 4) {
                winding_order = 0;
                ingest_poly(winding_order);
                vertex_list_delete_first(&VTX_LIST);
                vertex_list_delete_first(&VTX_LIST);
            }
            break;
    }
}

void GE::cmd_VTX_16()
{
    // unpack vertex and send it to our vertex stream
    //0-15 is X
    //16-31 is Y
    //next parm 0-15 Z
    printfcd("\nVTX_16: %f %f %f", vtx_to_float((i32)(i16)(DATA[0] & 0xFFFF)), vtx_to_float((i32)(i16)(DATA[0] >> 16)), vtx_to_float((i32)(i16)(DATA[1] & 0xFFFF)));
    params.vtx.x = (i16)(DATA[0] & 0xFFFF);
    params.vtx.y = (i16)(DATA[0] >> 16);
    params.vtx.z = (i16)(DATA[1] & 0xFFFF);
    ingest_vertex();
    //i32 x = DATA[0] &
}

void GE::cmd_VTX_DIFF()
{
    i16 xd = static_cast<i16>((DATA[0] & 0x3FF));
    i16 yd = static_cast<i16>((DATA[0] >> 10) & 0x3FF);
    i16 zd = static_cast<i16>((DATA[0] >> 20) & 0x3FF);
    xd <<= 6;
    yd <<= 6;
    zd <<= 6;
    xd >>= 6;
    yd >>= 6;
    zd >>= 6;
    params.vtx.x += xd;
    params.vtx.y += yd;
    params.vtx.z += zd;

    ingest_vertex();
}

void GE::cmd_VTX_XY()
{
    params.vtx.x = static_cast<i16>((DATA[0] & 0xFFFF));
    params.vtx.y = static_cast<i16>((DATA[0] >> 16));
    ingest_vertex();
}

void GE::cmd_VTX_XZ()
{
    params.vtx.x = static_cast<i16>(DATA[0] & 0xFFFF);
    params.vtx.z = static_cast<i16>(DATA[0] >> 16);
    ingest_vertex();
}

void GE::cmd_VTX_YZ()
{
    params.vtx.y = static_cast<i16>(DATA[0] & 0xFFFF);
    params.vtx.z = static_cast<i16>(DATA[0] >> 16);
    ingest_vertex();
}

void GE::cmd_VTX_10()
{
    params.vtx.x = (i16)((DATA[0] & 0x3FF) << 6);
    params.vtx.y = (i16)(((DATA[0] >> 10) & 0x3FF) << 6);
    params.vtx.z = (i16)(((DATA[0] >> 20) & 0x3FF) << 6);
    ingest_vertex();
}

void GE::cmd_POS_TEST()
{
    static int a = 1;
    if (a) {
        printf("\nWARN: POS TEST!");
        a = 0;
    }
    params.vtx.x = DATA[0] & 0xFFFF;
    params.vtx.y = DATA[0] >> 16;
    params.vtx.z = DATA[1] & 0xFFFF;

    if (clip_mtx_dirty) calculate_clip_matrix();
    i64 vertex[4] = {(i64)params.vtx.x, params.vtx.y, params.vtx.z, 1<<12};

    results.pos_test[0] = ((vertex[0]*M_CLIP[0] + vertex[1]*M_CLIP[4] + vertex[2]*M_CLIP[8] + vertex[3]*M_CLIP[12]) >> 12);
    results.pos_test[1] = ((vertex[0]*M_CLIP[1] + vertex[1]*M_CLIP[5] + vertex[2]*M_CLIP[9] + vertex[3]*M_CLIP[13]) >> 12);
    results.pos_test[2] = ((vertex[0]*M_CLIP[2] + vertex[1]*M_CLIP[6] + vertex[2]*M_CLIP[10] + vertex[3]*M_CLIP[14]) >> 12);
    results.pos_test[3] = ((vertex[0]*M_CLIP[3] + vertex[1]*M_CLIP[7] + vertex[2]*M_CLIP[11] + vertex[3]*M_CLIP[15]) >> 12);
}

static void box_test_clip_segment(i32 comp, i32 plane, VTX_list_node* outbuf, VTX_list_node* vin, VTX_list_node* vout)
{
    s64 factor_num = vin->data.xyzw[3] - (plane*vin->data.xyzw[comp]);
    s32 factor_den = factor_num - (vout->data.xyzw[3] - (plane*vout->data.xyzw[comp]));

#define INTERPOLATE(var)  { outbuf->data.var = (vin->data.var + ((vout->data.var - vin->data.var) * factor_num) / factor_den); }

    if (comp != 0) INTERPOLATE(xyzw[0]);
    if (comp != 1) INTERPOLATE(xyzw[1]);
    if (comp != 2) INTERPOLATE(xyzw[2]);
    INTERPOLATE(xyzw[3]);
    outbuf->data.xyzw[comp] = plane*outbuf->data.xyzw[3];

#undef INTERPOLATE
}

static int box_test_against_plane(int comp, VTX_list_node *vertices, int nverts) {
    VTX_list_node temp[10];
    i32 prev, next;
    i32 c = 0;

    for (i32 i = 0; i < nverts; i++) {
        prev = i - 1;
        if (prev < 0) prev = nverts - 1;
        next = i + 1;
        if (next >= nverts) next = 0;

        VTX_list_node *vtx = &vertices[i];
        if (vtx->data.xyzw[comp] > vtx->data.xyzw[3]) {
            VTX_list_node *vprev = &vertices[prev];
            if (vprev->data.xyzw[comp] <= vprev->data.xyzw[3]) {
                box_test_clip_segment(comp, 1, &temp[c], vtx, vprev);
                c++;
            }

            VTX_list_node *vnext = &vertices[next];
            if (vnext->data.xyzw[comp] <= vnext->data.xyzw[3]) {
                box_test_clip_segment(comp, 1, &temp[c], vtx, vnext);
                c++;
            }
        } else {
            memcpy(&temp[c++].data, &vtx->data, sizeof(VTX_list_node_data));
        }
    }

    nverts = c;
    c = 0;
    for (i32 i = 0; i < nverts; i++) {
        prev = i - 1;
        if (prev < 0) prev = nverts - 1;
        next = i + 1;
        if (next >= nverts) next = 0;

        VTX_list_node *vtx = &temp[i];
        if (vtx->data.xyzw[comp] < -vtx->data.xyzw[3]) {
            VTX_list_node *vprev = &temp[prev];
            if (vprev->data.xyzw[comp] >= -vprev->data.xyzw[3]) {
                box_test_clip_segment(comp, -1, &vertices[c], vtx, vprev);
                c++;
            }

            VTX_list_node *vnext = &temp[next];
            if (vnext->data.xyzw[comp] >= -vnext->data.xyzw[3]) {
                box_test_clip_segment(comp, -1, &vertices[c], vtx, vnext);
                c++;
            }
        } else {
            memcpy(&vertices[c++].data, &vtx->data, sizeof(VTX_list_node_data));
        }
    }

    return c;
}


static i32 box_test_clip_verts(VTX_list_node *vertices, int nverts)
{
    nverts = box_test_against_plane(2, vertices, nverts);

    // Y clipping
    nverts = box_test_against_plane(1, vertices, nverts);

    // X clipping
    nverts = box_test_against_plane(0, vertices, nverts);

    return nverts;
}

void GE::cmd_BOX_TEST()
{
    printf("\nBOX TEST!");
    VTX_list_node box[8];
    VTX_list_node face[10];

    GXSTAT.box_test_result = 0;

    i16 x0 = (i16)(DATA[0] & 0xFFFF);
    i16 y0 = ((i32)DATA[0]) >> 16;
    i16 z0 = (i16)(DATA[1] & 0xFFFF);
    i16 x1 = ((i32)DATA[1]) >> 16;
    i16 y1 = (i16)(DATA[2] & 0xFFFF);
    i16 z1 = ((i32)DATA[2]) >> 16;

    x1 += x0;
    y1 += y0;
    z1 += z0;

    box[0].data.xyzw[0] = x0;
    box[0].data.xyzw[1] = y0;
    box[0].data.xyzw[2] = z0;
    box[1].data.xyzw[0] = x1;
    box[1].data.xyzw[1] = y0;
    box[1].data.xyzw[2] = z0;
    box[2].data.xyzw[0] = x1;
    box[2].data.xyzw[1] = y1;
    box[2].data.xyzw[2] = z0;
    box[3].data.xyzw[0] = x0;
    box[3].data.xyzw[1] = y1;
    box[3].data.xyzw[2] = z0;
    box[4].data.xyzw[0] = x0;
    box[4].data.xyzw[1] = y1;
    box[4].data.xyzw[2] = z1;
    box[5].data.xyzw[0] = x0;
    box[5].data.xyzw[1] = y0;
    box[5].data.xyzw[2] = z1;
    box[6].data.xyzw[0] = x1;
    box[6].data.xyzw[1] = y0;
    box[6].data.xyzw[2] = z1;
    box[7].data.xyzw[0] = x1;
    box[7].data.xyzw[1] = y1;
    box[7].data.xyzw[2] = z1;

    if (clip_mtx_dirty) calculate_clip_matrix();
    for (int i = 0; i < 8; i++) {
        i32 x = box[i].data.xyzw[0];
        i32 y = box[i].data.xyzw[1];
        i32 z = box[i].data.xyzw[2];

        box[i].data.xyzw[0] = ((i64)x * M_CLIP[0] + (i64)y * M_CLIP[4] + (i64)z * M_CLIP[8] + (i64)0x1000 * M_CLIP[12]) >> 12;
        box[i].data.xyzw[1] = ((i64)x * M_CLIP[1] + (i64)y * M_CLIP[5] + (i64)z * M_CLIP[9] + (i64)0x1000 * M_CLIP[13]) >> 12;
        box[i].data.xyzw[2] = ((i64)x * M_CLIP[2] + (i64)y * M_CLIP[6] + (i64)z * M_CLIP[10] + (i64)0x1000 * M_CLIP[14]) >> 12;
        box[i].data.xyzw[3] = ((i64)x * M_CLIP[3] + (i64)y * M_CLIP[7] + (i64)z * M_CLIP[11] + (i64)0x1000 * M_CLIP[15]) >> 12;
    }

    // front face (-Z)
    face[0] = box[0]; face[1] = box[1]; face[2] = box[2]; face[3] = box[3];
    i32 res = box_test_clip_verts(face, 4);
    if (res > 0) {
        GXSTAT.box_test_result = 1;
        return;
    }

    // back face (+Z)*/
    face[0] = box[4]; face[1] = box[5]; face[2] = box[6]; face[3] = box[7];
    res = box_test_clip_verts(face, 4);
    if (res > 0) {
        GXSTAT.box_test_result = 1;
        return;
    }

    // left face (-X)
    face[0] = box[0]; face[1] = box[3]; face[2] = box[4]; face[3] = box[5];
    res = box_test_clip_verts(face, 4);
    if (res > 0) {
        GXSTAT.box_test_result = 1;
        return;
    }

    // right face (+X)
    face[0] = box[1]; face[1] = box[2]; face[2] = box[7]; face[3] = box[6];
    res = box_test_clip_verts(face, 4);
    if (res > 0) {
        GXSTAT.box_test_result = 1;
        return;
    }

    // bottom face (-Y)
    face[0] = box[0]; face[1] = box[1]; face[2] = box[6]; face[3] = box[5];
    res = box_test_clip_verts(face, 4);
    if (res > 0) {
        GXSTAT.box_test_result = 1;
        return;
    }

    // top face (+Y)
    face[0] = box[2]; face[1] = box[3]; face[2] = box[4]; face[3] = box[7];
    res = box_test_clip_verts(face, 4);
    if (res > 0) {
        GXSTAT.box_test_result = 1;
        return;
    }

}

void GE::cmd_SWAP_BUFFERS()
{
    printfcd("\n\n---------------------------\ncmd_SWAP_BUFFERS()");
    io.swap_buffers = 1;
    buffers[ge_has_buffer].translucent_y_sorting_manual = DATA[0] & 1;
    buffers[ge_has_buffer].depth_buffering_w = (DATA[0] >> 1) & 1;
}

void GE::cmd_POLYGON_ATTR()
{
    printfcd("\nPOLYGON_ATTR");
    params.poly.on_vtx_start.attr.u = DATA[0];
}

void GE::terminate_poly_strip()
{
    VTX_LIST.init();
    winding_order = 0;
}

void GE::cmd_VIEWPORT()
{
    re->io.viewport[0] = DATA[0] & 0xFF;
    re->io.viewport[1] = (191 - ((DATA[0] >> 8) & 0xFF)) & 0xFF;
    re->io.viewport[2] = (DATA[0] >> 16) & 0xFF;
    re->io.viewport[3] = (191 - (DATA[0] >> 24)) & 0xFF;
    re->io.viewport[4] = (re->io.viewport[2] - re->io.viewport[0] + 1) & 0x1FF;
    re->io.viewport[5] = (re->io.viewport[1] - re->io.viewport[3] + 1) & 0xFF;
}

void GE::cmd_TEXIMAGE_PARAM()
{
    printfcd("\nTEXIMAGE_PARAM");
    params.poly.current.tex_param.u = DATA[0];
}

void GE::cmd_BEGIN_VTXS()
{
    // Empty vertex cache, terminate strip...
    terminate_poly_strip();

    // Latch parameters. TODO: did I do them all?
    params.poly.current.attr.u = params.poly.on_vtx_start.attr.u;
    params.vtx_strip.mode = static_cast<P_MODE>(DATA[0] & 3);
    printfcd("\nBEGIN_VTXS(%d);", params.poly.current.attr.mode);
}

void GE::cmd_PLTT_BASE()
{
    params.poly.current.pltt_base = DATA[0] & 0x1FFF;
}

void GE::cmd_LIGHT_VECTOR()
{
    u32 l = DATA[0] >> 30;
    i16 dir[3];
    dir[0] = (i16)((DATA[0] & 0x000003FF) << 6) >> 6;
    dir[1] = (i16)((DATA[0] & 0x000FFC00) >> 4) >> 6;
    dir[2] = (i16)((DATA[0] & 0x3FF00000) >> 14) >> 6;
    lights.light[l].direction[0] = (-((dir[0] * M_VECTOR[0] + dir[1] * M_VECTOR[4] + dir[2] * M_VECTOR[8] ) >> 12) << 21) >> 21;
    lights.light[l].direction[1] = (-((dir[0] * M_VECTOR[1] + dir[1] * M_VECTOR[5] + dir[2] * M_VECTOR[9] ) >> 12) << 21) >> 21;
    lights.light[l].direction[2] = (-((dir[0] * M_VECTOR[2] + dir[1] * M_VECTOR[6] + dir[2] * M_VECTOR[10]) >> 12) << 21) >> 21;
    i32 den = -(((dir[0] * M_VECTOR[2] + dir[1] * M_VECTOR[6] + dir[2] * M_VECTOR[10]) << 9) >> 21) + (1 << 9);

    if (den == 0) lights.material_color.reciprocal[l] = 0;
    else lights.material_color.reciprocal[l] = (1 << 18) / den;

}


static void C15to15(u32 c, u32 *o)
{
    o[0] = (c & 0x1F);
    o[1] = ((c >> 5) & 0x1F);
    o[2] = ((c >> 10) & 0x1F);
}

void GE::cmd_DIF_AMB()
{
    C15to15(DATA[0] & 0x7FFF, lights.material_color.diffuse);
    C15to15((DATA[0] >> 16) & 0x7FFF, lights.material_color.ambient);
    if (DATA[0] & 0x8000) {
        params.vtx.color[0] = lights.material_color.diffuse[0];
        params.vtx.color[1] = lights.material_color.diffuse[1];
        params.vtx.color[2] = lights.material_color.diffuse[2];
    }
}

void GE::cmd_SPE_EMI()
{
    C15to15(DATA[0] & 0x7FFF, lights.material_color.reflection);
    C15to15((DATA[0] >> 16) & 0x7FFF, lights.material_color.emission);
    lights.shininess_enable = (DATA[0] >> 15) & 1;

}

void GE::cmd_LIGHT_COLOR() {
    u32 light_num = DATA[0] >> 30;
    C15to15(DATA[0] & 0x7FFF, lights.light[light_num].color);
}

static void do_cmd(void *ptr, u64 cmd, u64 current_clock, u32 jitter)
{

    auto *th = static_cast<NDS::GFX::GE *>(ptr);
    // Handle command!
    printfcd("\ndo_cmd %02llx", cmd);
    switch(cmd) {
#define dcmd(label) case CMD_##label: th->cmd_##label(); break
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
        dcmd(POS_TEST);
        dcmd(BOX_TEST);
#undef dcmd
        default:
            printf("\nUnhandled GE cmd %02llx", cmd);
            break;
    }

    th->GXSTAT.test_busy = 0;
    // Schedule next command if applicable!
    th->handle_cmd();
}

u32 GE::fifo_pop_full_cmd()
{
    if (!fifo.total_complete_cmds) {
        printfifo("\nFIFO POP FAIL NO CMDS!");
        return 0;
    }
    fifo.total_complete_cmds--;
    FIFO_entry *e = &fifo.items[fifo.head];
    u32 cmd = e->cmd;
    u32 num_params = tbl_num_params[cmd];
    printfifo("\nPOP THE HEAD & FIRST PARAM (CMD:%02x) FROM HEAD %d", cmd, fifo.head);
    fifo.head = (fifo.head + 1) & 511;
    fifo.len--;
    DATA[0] = e->data;
    u32 dpos = 1;
    for (u32 i = 1; i < num_params; i++) {
        printfifo("\nPOP A PARAMETER...");
        assert(fifo.len>0);
        e = &fifo.items[fifo.head];
        DATA[dpos++] = e->data;
        fifo.head = (fifo.head + 1) & 511;
        fifo.len--;
    }
    printfifo("\nFIFO POP RETURN %02x", cmd);
    return cmd;
}

void GE::handle_cmd()
{
    // Pop a command!
    // Schedule it to be completed in N cycles!
    printfifo("\nGE_HANDLE_CMD!");
    u32 cmd;
    // Drain the FIFO looking for a good command!
    if (io.swap_buffers) {
        printfifo("\nWAITING ON SWAP_BUFFERS");
        return;
    }
    GXSTAT.ge_busy = 0;
    if (fifo.total_complete_cmds == 0) {
        printfifo("\nGE_HANDLE_CMD: NO COMPLETE CMDS!");
        return;
    }

    cmd = fifo_pop_full_cmd();
    if (!cmd) {
        printfifo("\nGE_HANDLE_CMD: FIFO POP FULL FAILED!?");
        return;
    }
    printfifo("\nGE_HANDLE_CMD: HANDLE CMD %02x", cmd);
    GXSTAT.ge_busy = 1;
    i32 num_cycles = get_num_cycles(cmd);
    if (cmd == CMD_SWAP_BUFFERS) {
        num_cycles = 1;
    }

    fifo.update_len(false, false);
    assert(fifo.cmd_scheduled == 0);
    printfifo("\nSCHEDULING GE CMD %02x FOR %d CYCLES!", cmd, num_cycles);
    GXSTAT.test_busy = tbl_test_busy[cmd];
    scheduler->only_add_abs(bus->clock_current7() + num_cycles, cmd, this, &do_cmd, &fifo.cmd_scheduled);
}

u32 GE::check_irq() const {
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

void GE::write_fog_table(u32 addr, u8 sz, u32 val)
{
    if (sz >= 2) {
        write_fog_table(addr, 1, val & 0xFF);
        write_fog_table(addr+1, 1, (val >> 8) & 0xFF);
    }
    if (sz == 4) {
        write_fog_table(addr+2, 1, (val >> 16) & 0xFF);
        write_fog_table(addr+3, 1, (val >> 24) & 0xFF);
    }
    if (sz >= 2) return;
    re->io.FOG.TABLE[addr] = val;
}

void GE::write_edge_table(u32 addr, u8 sz, u32 val)
{
    if (sz == 4) {
        write_edge_table(addr, 1, val & 0xFFFF);
        write_edge_table(addr+2, 1, val >> 16);
        return;
    }
    if (sz >= 2) return;
    re->io.EDGE_TABLE_r[addr] = (val & 0x1F) << 1;
    re->io.EDGE_TABLE_g[addr] = ((val >> 5) & 0x1F) << 1;
    re->io.EDGE_TABLE_b[addr] = ((val >> 10) & 0x1F) << 1;

}

static u32 texc(u32 colo)
{
    //return ctabl[colo & 31];
    return ((colo & 0x1F) << 1) | ((colo & 0x10) >> 4);
}

void GE::write_toon_table(u32 addr, u8 sz, u32 val)
{
    if (sz == 4) {
        write_toon_table(addr, 1, val & 0xFFFF);
        write_toon_table(addr+2, 1, val >> 16);
        return;
    }
    re->io.TOON_TABLE_r[addr] = texc(val);
    re->io.TOON_TABLE_g[addr] = texc(val >> 5);
    re->io.TOON_TABLE_b[addr] = texc(val >> 10);;
}

void FIFO::push(u32 cmd, u32 val, bool from_dma)
{
    printfifo("\nFIFO PUSH cmd:%02x val:%08x", cmd, val);
    FIFO_entry *e = &items[tail];
    tail = (tail + 1) & 511;
    len++;

    e->cmd = cmd;
    e->data = val;
}

void FIFO::parse_cmd(u32 val, bool from_dma) {
    // parse 0-3 commands and how many args each has
    // push them to cmd_queue
    cmd_queue.len = 0;
    cmd_queue.head = 0;
    for (u32 i = 0; i < 4; i++) {
        if (val & 0xFF) {
            cmd_queue.items[cmd_queue.len++].cmd = val & 0xFF;
        }
        val >>= 8;
    }

    if (cmd_queue.len == 0) {
        printfifo("\nWAITING FOR CMD=1 (cmd was NOPs)");
        waiting_for_cmd = 1;
        return;
    }

    for (u32 i = 0; i < cmd_queue.len; i++) {
        u32 cmd = cmd_queue.items[i].cmd;
        assert(tbl_cmd_good[cmd]);
        cmd_queue.items[i].num_params_left = tbl_num_params[cmd];
        printfifo("\nCMD QUEUE %d cmd:%02x params:%d", i, cmd, cmd_queue.items[i].num_params_left);
    }

    while(cmd_queue.items[cmd_queue.head].num_params_left == 0) {
        printfifo("\nCMD WITH 0 PARAMS EARLY PUSH....");
        push(cmd_queue.items[cmd_queue.head].cmd, 0, from_dma);
        cmd_queue.len--;
        cmd_queue.head++;
        total_complete_cmds++;
        if (!cmd_queue.len) break;
    }

    if (cmd_queue.len == 0) {
        printfifo("\nWAITING FOR CMD=0 (all cmds pushed to FIFO)");
        waiting_for_cmd = 1;
        return;
    }

    printfifo("\nWAITING FOR CMD=0 (waiting for %d params)", cmd_queue.items[cmd_queue.head].num_params_left);
    waiting_for_cmd = 0;

    cur_cmd = cmd_queue.items[cmd_queue.head].cmd;
}

void FIFO::drain_cmd_queue(bool from_dma)
{
    while(cmd_queue.items[cmd_queue.head].num_params_left == 0) {
        cmd_queue.head++;
        cmd_queue.len--;
        total_complete_cmds++;
        if (cmd_queue.len > 0) {// If there are commands left...
            cur_cmd = cmd_queue.items[cmd_queue.head].cmd;
            if (cmd_queue.items[cmd_queue.head].num_params_left == 0) {
                push(cur_cmd, 0, from_dma);
            }
        }
        else {
            // All parameters have been passed...
            printfifo("\nWAITING FOR CMD = 1 (all params passed, num cmds: %d)", total_complete_cmds);
            waiting_for_cmd = 1;
            return;
        }
    }
}

void GE::fifo_write_direct(u32 val, bool from_dma, bool from_addr)
{
    if (fifo.waiting_for_cmd) {
        printfifo("\nPARSE CMD %02x", val);
        fifo.parse_cmd(val, 0);
    }
    else {
        // add parameters!
        // if parameter # = 0, destroy and move to next cmd!
        // if no cmd available, set wait for cmd to 1!
        fifo.push(fifo.cur_cmd, val, from_dma);
        fifo.cmd_queue.items[fifo.cmd_queue.head].num_params_left--;
        printfifo("\nPUSH PARAM %08x. NUM_LEFT: %d. FIFO_LEN:%d", val, fifo.cmd_queue.items[fifo.cmd_queue.head].num_params_left, fifo.len);
        fifo.drain_cmd_queue(from_dma);
    }

    fifo.update_len(from_dma, true);
    if (!from_addr && !GXSTAT.ge_busy) handle_cmd();
}

void GE::fifo_write_from_addr(CMDS cmd, u32 val)
{
    printfifo("\nADDR_CMD cmd:%02x val:%08x waiting:%d", cmd, val, fifo.waiting_for_cmd);
    if (fifo.waiting_for_cmd) fifo_write_direct(cmd, false, true);
    fifo_write_direct(val, false, true);
    if (!GXSTAT.ge_busy) handle_cmd();
}

void GE::write(u32 addr, u8 sz, u32 val)
{
    if ((addr >= R9_GXFIFO) && (addr < (R9_GXFIFO + 0x40))) {
        //printf("\nAddr %08x val %08x", addr, val);
        fifo_write_direct(val, 0, 0);
        return;
    }
    switch(addr) {
        case R9_ALPHA_TEST_REF:
            re->io.ALPHA_TEST_REF = val & 0xFF;
            return;
        case R9_CLRIMG_OFFSET:
            re->io.CLRIMAGE_OFFSET = val & 0xFFFF;
            return;
        case R9_FOG_OFFSET:
            re->io.FOG.OFFSET = val & 0xFFFF;
            return;
        case R9_FOG_COLOR:
            re->io.FOG.COLOR_r = (val & 0x1F) << 1;
            re->io.FOG.COLOR_g = ((val >> 5) & 0x1F) << 1;
            re->io.FOG.COLOR_b = ((val >> 10) & 0x1F) << 1;
        case R9_CLEAR_COLOR: {
            re->io.CLEAR.COLOR = c15to18(val & 0x7FFF);
            re->io.CLEAR.fog_to_rear_plane = (val >> 15) & 1;
            re->io.CLEAR.alpha = ((val >> 16) & 0x1F) << 1;
            if (re->io.CLEAR.alpha) re->io.CLEAR.alpha++;
            re->io.CLEAR.poly_id = (val >> 24) & 0x3F;
            return; }
        case R9_CLEAR_DEPTH:
            assert(sz==2);
            val &= 0x7FFF;
            re->io.CLEAR.depth = ((val << 9) + ((val + 1) >> 15)) * 0x1FF;
            return;
        case R9_GXSTAT:
            assert(sz==4);
            if (val & 0x8000) {
                GXSTAT.matrix_stack_over_or_underflow_error = 0;
                GXSTAT.projection_matrix_stack_level = 0;
                matrices.stacks.position_vector_ptr = 0;
                matrices.stacks.texture_ptr = 0;
            }
            GXSTAT.cmd_fifo_irq_mode = (val >> 30) & 3;
            if (check_irq())
                bus->update_IF9(IRQ_GXFIFO);
            return;


#define gcmd(label) case R9_G_CMD_##label: fifo_write_from_addr(CMD_##label, val); return
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
        write_edge_table(addr, sz, val);
        return;
    }
    if ((addr >= R9_FOG_TABLE) && (addr < (R9_FOG_TABLE + 0x20))) {
        addr = addr - R9_FOG_TABLE;
        assert(addr<32);
        write_fog_table(addr, sz, val);
        return;
    }
    if ((addr >= R9_TOON_TABLE) && (addr < (R9_TOON_TABLE + 0x40))) {
        addr = (addr - R9_TOON_TABLE) >> 1;
        assert(addr<32);
        write_toon_table(addr, sz, val);
        return;
    }

    printf("\nUnhandled write to GE addr:%08x sz:%d val:%08x", addr, sz, val);
}

u32 GE::read_results(u32 addr, u8 sz)
{
    if ((addr >= 0x04000630) && (addr < 0x04000638)) {
        static int a = 1;
        if (a) {
            printf("\nVEC TEST READ");
            a = 0;
        }
        u32 which = (addr - 0x04000630) >> 2;
        assert(which<2);
        return results.vector[which];
    }
    if ((addr >= 0x04000640) && (addr < 0x04000680)) {
        static int a = 1;
        if (a) {
            printf("\nCLIPMATRIX READ!");
            a = 0;
        }
        if (clip_mtx_dirty) calculate_clip_matrix();
        return M_CLIP[(addr & 0x3F) >> 2];
    }
    printf("\nUNHANDLED READ FROM RESULTS AT %08x", addr);
    return 0;
}

u32 GE::read(u32 addr, u8 sz)
{
    u32 v;
    switch(addr) {
        case R9_RAM_COUNT:
            v = buffers[ge_has_buffer].polygon_index;
            if (sz == 4) v |= buffers[ge_has_buffer].vertex_index << 16;
            return v;
        case R9_RAM_COUNT+2:
            return buffers[ge_has_buffer].vertex_index;
        case R9_G_CMD_POLYGON_ATTR:
            return 0;
        case R9_GXSTAT:
            assert(sz==4);
            return GXSTAT.u;
        case 0x04000620: return results.pos_test[0];
        case 0x04000624: return results.pos_test[1];
        case 0x04000628: return results.pos_test[2];
        case 0x0400062C: return results.pos_test[3];

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
        return read_results(addr, sz);
    }
    printf("\nUnhandled read from GE addr:%08x sz:%d", addr, sz);
    return 0;
}

static void do_swap_buffers(void *ptr, u64 key, u64 current_clock, u32 jitter)
{
    printfifo("\n\ndo_swap_buffers()!!");
    auto *th = static_cast<NDS::GFX::GE *>(ptr);
    th->io.swap_buffers = 0;
    th->ge_has_buffer ^= 1;
    th->buffers[th->ge_has_buffer].polygon_index = 0;
    th->buffers[th->ge_has_buffer].vertex_index = 0;
    th->re->io.DISP3DCNT.poly_vtx_ram_overflow = 0;
    th->re->render_frame();
    th->handle_cmd();
}

void GE::vblank_up()
{
    // Schedule swap_buffers if we're scheduled to!
    if (io.swap_buffers) {
        scheduler->only_add_abs(bus->clock_current7() + 392, 0, this, &do_swap_buffers, nullptr);
    }
    else {
        re->render_frame();
    }
}
}