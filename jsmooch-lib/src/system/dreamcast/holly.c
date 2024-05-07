//
// Created by Dave on 2/16/2024.
//

/*
 * Thanks very much to Senrokou of the Deecey emulator, who graciously let
 *  me use Deecey as a base for my polygon TA FIFO parsing code
 */


#include <string.h>
#include "assert.h"
#include "stdio.h"
#include "stdlib.h"
#include "helpers/debug.h"
#include "dreamcast.h"
#include "holly.h"
#include "triangle.h"
#include "component/cpu/sh4/sh4_interrupts.h"
#include "helpers/multisize_memaccess.c"

void DCDisplayList_reset(struct DCDisplayList* this);

static void holly_soft_reset(struct DC* this)
{
    printf("\nHOLLY soft reset!");
    this->holly.ta.cmd_buffer_index = 0;
    this->holly.ta.list_type = HPL_none;
    this->holly.ta.cur_poly.valid = 0;
    this->holly.ta.cur_volume.valid = 0;
    this->holly.ta.user_tile_clip.valid = 0;
    cvec_clear(&this->holly.ta.volume_triangles);
    cvec_clear(&this->holly.ta.opaque_modifier_volumes);
    cvec_clear(&this->holly.ta.translucent_modifier_volumes);
    for (u32 i = 0; i < 5; i++)
        DCDisplayList_reset(&this->holly.ta.display_lists[i]);
}

#define NI 0b1111
static u32 HOLLY_IRQ_outputs[7] = {
        NI, // no interrupt
        NI, // no interrupt
        0b1101, // level 2
        NI, // no intterupt
        0b1011, // level 4
        NI, // no interrupt
        0b1001  // level 6
};
#undef NI

void holly_recalc_interrupts(struct DC* this)
{
    u32 level2 = (this->io.SB_IML2NRM & this->io.SB_ISTNRM.u) & 0x3FFFFF;
    level2 |= (this->io.SB_IML2EXT.u & this->io.SB_ISTEXT.u);
    level2 |= (this->io.SB_IML2ERR.u & this->io.SB_ISTERR.u);

    u32 level4 = (this->io.SB_IML4NRM & this->io.SB_ISTNRM.u);
    level4 |= (this->io.SB_IML4EXT.u & this->io.SB_ISTEXT.u);
    level4 |= (this->io.SB_IML4ERR.u & this->io.SB_ISTERR.u);

    u32 level6 = (this->io.SB_IML6NRM & this->io.SB_ISTNRM.u);
    level6 |= (this->io.SB_IML6EXT.u & this->io.SB_ISTEXT.u);
    level6 |= (this->io.SB_IML6ERR.u & this->io.SB_ISTERR.u);

    u32 highest_level = 0;
    if (level2) highest_level = 2;
    if (level4) highest_level = 4;
    if (level6) highest_level = 6;
    if ((highest_level != 0) && (dbg.trace_on)) {
        dbg_printf(DBGC_RED "\nHIGHEST LEVEL: %d l2:%04x l4:%04x l6:%04x cyc:%llu" DBGC_RST, highest_level, this->io.SB_IML2NRM, this->io.SB_IML4NRM, this->io.SB_IML6NRM, this->sh4.clock.trace_cycles);
    }
    //if (highest_level != this->sh4.IRL_irq_level) {
        //printf("\nINTERRUPT HIGHEST LEVEL CHANGE TO %d cyc:%llu", highest_level, this->sh4.clock.trace_cycles);
        //printf("\nIML6NRN: %08x", this->io.SB_IML6NRM & this->io.SB_ISTNRM.u & 0x3FFFFF);
    //}
    //printf("\nHOLLY RAISING INTERRUPT ON CYCLE %llu", this->sh4.clock.trace_cycles);
    u32 lv = HOLLY_IRQ_outputs[highest_level];
#ifdef DCDBG_HOLLY_IRQ
    printf("\nSET HOLLY IRQ OUT LEVEL TO %d", lv);
#endif
    SH4_set_IRL_irq_level(&this->sh4, lv);
}

void holly_eval_interrupt(struct DC* this, enum holly_interrupt_masks irq_num, u32 is_true)
{
    if (is_true) holly_raise_interrupt(this, irq_num, -1);
    else holly_lower_interrupt(this, irq_num);
}

void holly_lower_interrupt(struct DC* this, enum holly_interrupt_masks irq_num)
{
    u32 imask = (1 << (irq_num & 0xFF)) ^ 0xFFFFFFFF;
    if (irq_num & 0x100)
        this->io.SB_ISTEXT.u &= imask;
    else if (irq_num & 0x200)
        this->io.SB_ISTERR.u &= imask;
    else
        this->io.SB_ISTNRM.u &= imask;
    holly_recalc_interrupts(this);
}
/*
    hirq_vblank_in = 4,
    hirq_vblank_out = 5,

    hirq_maple_dma = holly_nrm | 12,

    hirq_gdrom_cmd = holly_ext | 0
*/
static const char irq_strings[50][50] = {
        "gdrom_cmd",
        "",
        "",
        "",
        "hirq_vblank_in", // 4
        "hirq_vblank_out", // 5
        "",
        "",
        "",
        "",
        "",
        "",
        "holly_maple_dma", // 12
        "",
        "",
        "",
        "",
        "",
        "",
        "Ch2DMA END", // 19
};

struct delay_func_struct {
    struct DC* DC;
    enum holly_interrupt_masks which;
};

u32 holly_delayed_raise_interrupt(void* ptr, u64 key, i64 clock, u32 jitter)
{
    struct DC* this = (struct DC*)ptr;
    holly_raise_interrupt(this, key, 0);
    return 0;
}


void holly_raise_interrupt(struct DC* this, enum holly_interrupt_masks irq_num, i64 delay)
{
    if (delay > 0) {
        struct scheduled_bound_function* bf = scheduler_bind_function(&holly_delayed_raise_interrupt, this);
        scheduler_add(&this->scheduler, (i64)this->sh4.clock.trace_cycles + delay, SE_bound_function, irq_num, bf);
        return;
    }
    u32 imask = 1 << (irq_num & 0xFF);
    if (irq_num & 0x100)
        this->io.SB_ISTEXT.u |= imask;
    else if (irq_num & 0x200)
        this->io.SB_ISTERR.u |= imask;
    else
        this->io.SB_ISTNRM.u |= imask;
#ifdef SH4_IRQ_DBG
    printf(DBGC_BLUE "\nHOLLY RAISE INTTERUPT %s val:%08x SB_ISTNRM:%08x SB_ISTEXT:%08x cyc:%llu" DBGC_RST, irq_strings[irq_num & 0xFF], imask, this->io.SB_ISTNRM.u, this->io.SB_ISTEXT.u, this->sh4.clock.trace_cycles);
#endif
    holly_recalc_interrupts(this);
}

void DC_recalc_frame_timing(struct DC* this)
{
    // We need to know:
    // kind line to vblank in IRQ. cycle # in frame
    // kind line to vblank out IRQ. cycle # in frame
    // how many cycles per line
    // how many lines in frame
    //this->clock.cycles_per_frame;
    this->clock.cycles_per_line = this->clock.cycles_per_frame / this->holly.SPG_LOAD.vcount;
    this->clock.interrupt.vblank_in_start = this->holly.SPG_VBLANK_INT.vblank_in_line * this->clock.cycles_per_line;
    this->clock.interrupt.vblank_out_start = this->holly.SPG_VBLANK_INT.vblank_out_line * this->clock.cycles_per_line;
}

#define B32(b31_b28, b27_24,b23_20,b19_16,b15_12,b11_8,b7_4,b3_0) ( \
  ((0b##b31_b28) << 28) | ((0b##b27_24) << 24) | \
  ((0b##b23_20) << 20) | ((0b##b19_16) << 16) | \
  ((0b##b15_12) << 12) | ((0b##b11_8) << 8) | \
  ((0b##b7_4) << 4) | (0b##b3_0))

#define B10_6_10 B32(0000,0011,1111,1111,0000,0011,1111,1111)

static void holly_TA_list_init(struct DC* this)
{
    printf("\nTA LIST INIT!");

    if (!this->holly.TA_LIST_CONT) {
        this->holly.TA_NEXT_OPB = this->holly.TA_NEXT_OPB_INIT;
        this->holly.TA_ITP_CURRENT = this->holly.TA_ISP_BASE;
    }
    this->holly.ta.cmd_buffer_index = 0;
    this->holly.ta.list_type = HPL_none;
    this->holly.ta.cur_volume.valid = 0;
    this->holly.ta.cur_poly.valid = 0;
    this->holly.ta.user_tile_clip.valid = 0;

    cvec_clear(&this->holly.ta.opaque_modifier_volumes);
    cvec_clear(&this->holly.ta.translucent_modifier_volumes);
    cvec_clear(&this->holly.ta.volume_triangles);
}

static u32 holly_get_frame_cycle(struct DC* this) {
    return (u32)(this->clock.frame_start_cycle - this->sh4.clock.trace_cycles);
}

static u32 holly_get_SPG_line(struct DC* this) {
    u32 cycle_num = holly_get_frame_cycle(this);
    return cycle_num / this->clock.cycles_per_line;
}

u64 holly_read(struct DC* this, u32 addr, u32* success) {
    *success = 1;
    u32 v;
    switch ((addr & 0x0000FFFF) | 0x005F0000) {
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/holly_reads.c"
        case 0x005F8000: // Device ID
            return 0x17FD11DB;
        case 0x005F80004: // Revision
            return 0x0011;
        case 0x005f8144: // TA_LIST_INIT
            return 0;
        case 0x005F8004: // REVISION
            return 0x0011;
        case 0x005F810C: // SPG_STATUS read-only
            // determine scanline
            v = holly_get_SPG_line(this) & 0x3FF;
            //TODO: blank, hsync, fieldnum
            v |= (this->clock.in_vblank) << 13;
            return v;
    }

    printf("\nUNKNOWN HOLLY READ: %08x", addr);
    fflush(stdout);
    *success = 0;
    return 0;
}

void holly_write(struct DC* this, u32 addr, u32 val, u32* success)
{
    *success = 1;
    addr = (addr & 0x0000FFFF) | 0x005F0000;
    if ((addr >= 0x005F8200) && (addr <= 0x005F83FC)) {
        this->holly.FOG_TABLE[(addr - 0x005F8200) >> 2] = val;
        return;
    }
    switch(addr) {
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/holly_writes.c"
        case 0x005f8144: // TA_LIST_INIT
            if (val & 0x80000000) holly_TA_list_init(this);
            return;
        case 0x005f8078: // FPU_CULL_VAL (float)
            this->holly.FPU_CULL_VAL = *(float*)&val;
            return;
        case 0x005f8084: // FPU_PERP_VAL (float)
            this->holly.FPU_PERP_VAL = *(float*)&val;
            return;
        case 0x005f8088: // ISP_BACKGND_D (float)
            this->holly.ISP_BACKGND_D = *(float*)&val;
            return;
        case 0x005F8008: // SOFTRESET
            holly_soft_reset(this);
            return;
    }

    *success = 0;
    printf("\nUNKNOWN HOLLY WRITE: %08x data:%08x cyc:%llu", addr, val, this->sh4.clock.trace_cycles);
    fflush(stdout);
}

void holly_vblank_in(struct DC* this)
{
    this->clock.in_vblank = 1;
    this->io.SB_ISTNRM.vblank_in = 1;
    holly_raise_interrupt(this, hirq_vblank_in, -1);
}

void holly_vblank_out(struct DC* this) {
    this->clock.in_vblank = 0;
    this->io.SB_ISTNRM.vblank_out = 1;
    holly_raise_interrupt(this, hirq_vblank_out, -1);
    this->holly.ta.list_type = HPL_none;

    if ((this->maple.SB_MDTSEL == 1) && this->maple.SB_MDEN) {
        maple_dma_init(this);
    }
}

void DCDisplayList_reset(struct DCDisplayList* this)
{
    cvec_clear(&this->vertex_parameters);
    cvec_clear(&this->vertex_strips);
    this->next_first_vertex_parameters_index = 0;
}

enum VolumeInstruction {
    VI_Normal = 0,
    VI_InsideLastPolygon = 1,
    VI_OutsideLastPolygon = 2,
};

struct DCGenericGlobalParameter {
    union HOLLY_PCW pcw;
    union HOLLY_ISP_TSP_IWORD isp_tsp_word;
    union HOLLY_TSP_IWORD tsp_word;
    union HOLLY_TEX_CTRL texture_ctrl;
} __attribute__((packed));

static void holly_TA_check_end_of_modifier_volume(struct DC* this)
{
    if ((this->holly.ta.list_type == HPL_opaque_mv) || (this->holly.ta.list_type == HPL_translucent_mv)) {
        if (this->holly.ta.cur_volume.valid) {
                // from Deecey FIXME: I should probably honor _ta_user_tile_clip here too... Given the examples in the doc, modifier volume can also be clipped.

                this->holly.ta.cur_volume.triangle_count = this->holly.ta.volume_triangles.len - this->holly.ta.cur_volume.first_triangle_index;
                if (this->holly.ta.cur_volume.triangle_count > 0) {
                    if (this->holly.ta.list_type == HPL_opaque_mv) {
                        cvec_push_back_copy(&this->holly.ta.opaque_modifier_volumes, &this->holly.ta.cur_volume);
                    } else {
                        cvec_push_back_copy(&this->holly.ta.translucent_modifier_volumes, &this->holly.ta.cur_volume);
                    }
                }
                this->holly.ta.cur_volume.valid = 0;
        }
    }
}

static void holly_TA_handle_object_list_set(struct DC* this) {
    union HOLLY_PCW parameter_control_word;
    parameter_control_word.u = *(u32 *) &this->holly.ta.cmd_buffer[0];
    assert(parameter_control_word.para_type == ctrl_object_list_set);

    if (this->holly.ta.list_type == HPL_none) {
        printf("\nLIST TYPE SET! %d", parameter_control_word.list_type);
        this->holly.ta.list_type = parameter_control_word.list_type;
        DCDisplayList_reset(&this->holly.ta.display_lists[this->holly.ta.list_type]);
    }
    printf(DBGC_RED "\nUnimplemented ObjectListSet" DBGC_RST);
    // FIXME: Really not sure if I need to do any thing here...
    //        Is it meant to separate objects by tiles? Are they already submitted elsewhere anyway?
    if (false) {
        /*const object_list_set = @as(*ObjectListSet, @ptrCast(&this->holly.ta.command_buffer)).*;
        const param_base = this->_get_register(u32, .PARAM_BASE).*;
        const ta_alloc_ctrl = this->_get_register(TA_ALLOC_CTRL, .TA_ALLOC_CTRL).*;
        std.debug.assert(ta_alloc_ctrl.OPB_Mode == 0);
        const addr = 4 * object_list_set.object_pointer; // 32bit word address
        while (true) {
            const object = @as(*u32, @ptrCast(&this->vram[addr])).*;
            if (object & 0x80000000 == 0) {
                // Triangle Strip
                const strip_addr = param_base + 4 * (object & 0x1FFFFF);
                _ = strip_addr;
            } else {
                switch ((object >> 29) & 0b11) {
                    0b00 => {
                        // Triangle Array
                        @panic("Unimplemented Triangle Array");
                    },
                    0b01 => {
                        // Quad Array
                        @panic("Unimplemented Quad Array");
                    },
                    0b11 => {
                        std.debug.assert(object & 0b11 == 0);
                        // Object Pointer Block Link
                        if (object & 0x10000000 == 0x10000000) {
                            // End of list
                            break;
                        } else {
                            @panic("Unimplemented Object Pointer Block Link");
                        }
                    },
                    else => {
                        @panic("Invalid Object type");
                    },
                }
            }
            addr += 4;
        }*/
    }
}


static enum DCVertexParamKind obj_control_to_vertex_parameter_format(union HOLLY_PCW pcw) {
    u32 bit_masked = pcw.u & 0x79;
    switch(bit_masked) {
        case 0x0:
            return DCVP0;
        case 0x1:
            return DCVP0;
        case 0x10:
            return DCVP1;
        case 0x20:
            return DCVP2;
        case 0x30:
            return DCVP2;
        case 0x8:
            return DCVP3;
        case 0x9:
            return DCVP4;
        case 0x18:
            return DCVP5;
        case 0x19:
            return DCVP6;
        case 0x28:
            return DCVP7;
        case 0x38:
            return DCVP7;
        case 0x29:
            return DCVP8;
        case 0x39:
            return DCVP8;
        case 0x40:
            return DCVP9;
        case 0x60:
            return DCVP10;
        case 0x70:
            return DCVP10;
        case 0x48:
            return DCVP11;
        case 0x49:
            return DCVP12;
        case 0x68:
            return DCVP13;
        case 0x78:
            return DCVP13;
        case 0x69:
            return DCVP14;
        case 0x79:
            return DCVP14;
    }
    printf("\nWHAT!?!?!??!");
    return 0;
}

u32 vertex_parameter_size(enum DCVertexParamKind kind) {
    switch(kind) {
        case DCVP0: return 256/8;
        case DCVP1: return 256/8;
        case DCVP2: return 256/8;
        case DCVP3: return 256/8;
        case DCVP4: return 256/8;
        case DCVP5: return 512/8;
        case DCVP6: return 512/8;
        case DCVP7: return 256/8;
        case DCVP8: return 256/8;
        case DCVP9: return 256/8;
        case DCVP10: return 256/8;
        case DCVP11: return 512/8;
        case DCVP12: return 512/8;
        case DCVP13: return 512/8;
        case DCVP14: return 512/8;
        case DCVPSP0: return 512/8;
        case DCVPSP1: return 512/8;
        default:
            printf("\nUNKNOWN VERTEX PARAMETER FORMAT WTF %d", kind);
            printf("\nUNKNOWN VERTEX PARAMETER FORMAT WTF %d", kind);
            printf("\nUNKNOWN VERTEX PARAMETER FORMAT WTF %d", kind);
            printf("\nUNKNOWN VERTEX PARAMETER FORMAT WTF %d", kind);
            printf("\nUNKNOWN VERTEX PARAMETER FORMAT WTF %d", kind);
            return 0;
    }
}

static enum DCPolyKind obj_control_to_polygon_format(union HOLLY_PCW pcw) {
    u32 bit_masked = pcw.u & 0x7D; // Thanks Deecey! Except odd ones can't happen...TODO: verify?
    switch(bit_masked) {
        /* THANK YOU DEECEY! */
        case 0x0: return DCP_type0;
        case 0x1: return DCP_type0;
        case 0x4: return DCP_type0;
        case 0x5: return DCP_type0;
        case 0x10: return DCP_type0;
        case 0x20: return DCP_type1;
        case 0x30: return DCP_type0;
        case 0x40: return DCP_type3;
        case 0x60: return DCP_type4;
        case 0x70: return DCP_type3;
        case 0x8: return DCP_type0;
        case 0xc: return DCP_type0;
        case 0x9: return DCP_type0;
        case 0xd: return DCP_type0;
        case 0x18: return DCP_type0;
        case 0x1c: return DCP_type0;
        case 0x19: return DCP_type0;
        case 0x1d: return DCP_type0;
        case 0x28: return DCP_type1;
        case 0x2c: return DCP_type2;
        case 0x29: return DCP_type1;
        case 0x2d: return DCP_type2;
        case 0x38: return DCP_type0;
        case 0x3c: return DCP_type0;
        case 0x39: return DCP_type0;
        case 0x3d: return DCP_type0;
        case 0x48: return DCP_type3;
        case 0x4c: return DCP_type3;
        case 0x49: return DCP_type3;
        case 0x4d: return DCP_type3;
        case 0x68: return DCP_type4;
        case 0x6c: return DCP_type4;
        case 0x69: return DCP_type4;
        case 0x6d: return DCP_type4;
        case 0x78: return DCP_type3;
        case 0x7c: return DCP_type3;
        case 0x79: return DCP_type3;
        case 0x7d: return DCP_type3;
    }
}

static u32 polygon_format_size(enum DCPolyKind kind)
{
    switch(kind) {
        case DCP_type0:
            return 256/8;
        case DCP_type1:
            return 256/8;
        case DCP_type2:
            return 512/8;
        case DCP_type3:
            return 256/8;
        case DCP_type4:
            return 512/8;
        default:
            assert(1==0);
            printf("\nINVALID SIZERRR");
            return -1;
    }
}

static void set_ta_poly(struct DC* this, enum DCPolyKind kind, u8* buf)
{
/*
                    this->holly.ta.current_polygon = switch (format) {
                        .PolygonType0 => .{ .PolygonType0 = @as(*PolygonType0, @ptrCast(&this->holly.ta.command_buffer)).* },
                        .PolygonType1 => .{ .PolygonType1 = @as(*PolygonType1, @ptrCast(&this->holly.ta.command_buffer)).* },
                        .PolygonType2 => .{ .PolygonType2 = @as(*PolygonType2, @ptrCast(&this->holly.ta.command_buffer)).* },
                        .PolygonType3 => .{ .PolygonType3 = @as(*PolygonType3, @ptrCast(&this->holly.ta.command_buffer)).* },
                        .PolygonType4 => .{ .PolygonType4 = @as(*PolygonType4, @ptrCast(&this->holly.ta.command_buffer)).* },
                        else => @panic("Invalid polygon format"),
                    };
 */
    this->holly.ta.cur_poly.valid = 1;
    this->holly.ta.cur_poly.kind = kind;
    memcpy(&this->holly.ta.cur_poly.data, &this->holly.ta.cmd_buffer[0], polygon_format_size(kind));
}

void holly_TA_cmd(struct DC* this) {

    if (this->holly.ta.cmd_buffer_index % 8 != 0) return; // All commands are 8 or 16 bytes long

    union HOLLY_PCW cmd;
    cmd.u = *(u32 *)(&this->holly.ta.cmd_buffer[0]);
    enum HOLLY_PCW_paratype ptype = cmd.para_type;
    printf("\nWORKING ON LIST %d", this->holly.ta.list_type);

    switch(ptype) {
        case ctrl_end_of_list:
            printf("\nEND OF LIST!");
            holly_TA_check_end_of_modifier_volume(this);
            switch(this->holly.ta.list_type) {
                case HPL_opaque:
                    holly_raise_interrupt(this, hirq_opaque_list, 800);
                    break;
                case HPL_opaque_mv:
                    holly_raise_interrupt(this, hirq_opaque_modifier_list, 800);
                    break;
                case HPL_translucent:
                    holly_raise_interrupt(this, hirq_translucent_list, 800);
                    break;
                case HPL_translucent_mv:
                    holly_raise_interrupt(this, hirq_translucent_modifier_list, 800);
                    break;
                case HPL_punchthru:
                    holly_raise_interrupt(this, hirq_punchthru, 800);
                    break;
                case HPL_none:
                    break;
                default:
                    printf("\nIMPOSSIBLE!");
                    break;
            }
            this->holly.ta.cur_poly.valid = 0;
            this->holly.ta.list_type = HPL_none;
            break;
        case ctrl_user_tile_clip:
            printf("\nUSER TILE CLIP!");
            struct DCUserTileClip *user_tile_clip = (struct DCUserTileClip*)&this->holly.ta.cmd_buffer[0];
            this->holly.ta.user_tile_clip = (struct DCUserTileClipInfo) {
                    .valid = 1,
                    .user_clip_usage = DCUCU_disable,
                    .x = 32 * user_tile_clip->user_clip_x_min,
                    .y = 32 * user_tile_clip->user_clip_y_min,
                    .width = 32 * (1 + user_tile_clip->user_clip_x_max - user_tile_clip->user_clip_x_min),
                    .height = 32 * (1 + user_tile_clip->user_clip_y_max - user_tile_clip->user_clip_y_min),
            };
            break;
        case ctrl_object_list_set:
            printf("\nOBJ LIST SET!");
            holly_TA_handle_object_list_set(this);
            break;
        case ctrl_reserved:
            printf("\nRESERVED!");
            break;
        case global_polygon_or_modifier_volume:
            printf("\nPOLY OR MODIFIER VOLUME!");
            if (this->holly.ta.list_type == HPL_none) {
                printf("\nSET LIST TYPE %d", cmd.list_type);
                this->holly.ta.list_type = cmd.list_type;
                assert(this->holly.ta.list_type < 5);
                DCDisplayList_reset(&this->holly.ta.display_lists[this->holly.ta.list_type]);
            }

            if (cmd.group_en == 1) {
                if (this->holly.ta.user_tile_clip.valid) {
                    this->holly.ta.user_tile_clip.user_clip_usage = cmd.user_clip;
                }
            }

            if ((this->holly.ta.list_type == HPL_opaque_mv) || (this->holly.ta.list_type == HPL_translucent_mv)) {
                union HOLLY_MVI modifier_volume;
                modifier_volume.u = *(u32 *)(&this->holly.ta.cmd_buffer[0]); // TODO: is this correct?
                // New modifier volume starts
                holly_TA_check_end_of_modifier_volume(this);
                if (!this->holly.ta.cur_volume.valid) {
                    this->holly.ta.cur_volume = (struct DCModifierVolume) {
                            .valid = 1,
                            .parameter_control_word = cmd,
                            .instructions = modifier_volume,
                            .first_triangle_index = this->holly.ta.volume_triangles.len
                    };
                    if (modifier_volume.volume_instruction == VI_OutsideLastPolygon) {
                        printf("\nunsupported exclusion modifier volume.");
                    }
                }
            } else {
                // NOTE: "Four bits in the ISP/TSP Instruction Word are overwritten with the corresponding bit values from the Parameter Control Word."
                struct DCGenericGlobalParameter* global_parameter = (struct DCGenericGlobalParameter*)(&this->holly.ta.cmd_buffer[0]);

                global_parameter->isp_tsp_word.texture = global_parameter->pcw.texture;
                global_parameter->isp_tsp_word.offset = global_parameter->pcw.offset;
                global_parameter->isp_tsp_word.gouraud = global_parameter->pcw.gouraud;
                global_parameter->isp_tsp_word.uv_16bit = global_parameter->pcw.uv_is_16bit;

                enum DCPolyKind format = obj_control_to_polygon_format(cmd);
                if (this->holly.ta.cmd_buffer_index < polygon_format_size(format)) return;

                set_ta_poly(this, format, this->holly.ta.cmd_buffer);
            }
            break;
        case global_sprite:
            printf("\nSPRITE!");
            if (this->holly.ta.list_type == HPL_none) {
                this->holly.ta.list_type = cmd.list_type;
                DCDisplayList_reset(&this->holly.ta.display_lists[this->holly.ta.list_type]);
            }

            if (cmd.group_en == 1) {
                if (this->holly.ta.user_tile_clip.valid) {
                    this->holly.ta.user_tile_clip.user_clip_usage = cmd.user_clip;
                }
            }

            set_ta_poly(this, DCP_sprite, &this->holly.ta.cmd_buffer[0]);
            break;
        case global_reserved:
            printf("\nGLOBAL RESERVED!");
            break;
        case vertex_parameter:
            printf("\nVERTEX PARAMETER!!!");
            assert(this->holly.ta.list_type != 10);
            struct DCDisplayList* display_list = &this->holly.ta.display_lists[this->holly.ta.list_type];

            if ((this->holly.ta.list_type == HPL_opaque_mv) || (this->holly.ta.list_type == HPL_translucent_mv)) {
                if (this->holly.ta.cmd_buffer_index < sizeof(struct DCModifierVolumeParameter)) return;
                struct DCModifierVolumeParameter *mvp = cvec_push_back(&this->holly.ta.volume_triangles);
                memcpy(mvp, &this->holly.ta.cmd_buffer[0], sizeof(struct DCModifierVolumeParameter));

                // NOTE: I feel like the documentation contradicts itself here, volume == 1 is said to indicate the last triangle of a modifier volume,
                //       but the example use an extra global parameter to end the modifier volume. The later seem correct in practice.
                //           if (parameter_control_word.obj_control.volume == 1) end_volume()?
            } else {
                if (this->holly.ta.cur_poly.valid == 0) {
                    printf("\n    No current polygon! Current list type: %d", this->holly.ta.list_type);
                }

                struct DCGenericGlobalParameter* polygon_obj_control = (struct DCGenericGlobalParameter* )&this->holly.ta.cur_poly.data[0];
                switch (this->holly.ta.cur_poly.kind) {
                    case DCP_sprite: {
                        assert(cmd.end_of_strip == 1); // Sanity check: For Sprites/Quads, each vertex parameter describes an entire polygon.
                        struct DCVertexParam* vp = (struct DCVertexParam *)cvec_push_back(&display_list->vertex_parameters);
                        if (polygon_obj_control->texture_ctrl.u == 0) {
                            if (this->holly.ta.cmd_buffer_index < vertex_parameter_size(DCVPSP0)) return;
                            vp->kind = DCVPSP0;
                        }
                        else {
                            if (this->holly.ta.cmd_buffer_index < vertex_parameter_size(DCVPSP1)) return;
                            vp->kind = DCVPSP1;
                        }
                        memcpy(vp->data, &this->holly.ta.cmd_buffer[0], vertex_parameter_size(vp->kind));
                        break;
                    }
                    default: {
                        enum DCVertexParamKind format = obj_control_to_vertex_parameter_format(cmd);
                        if (this->holly.ta.cmd_buffer_index < vertex_parameter_size(format)) return;
                        struct DCVertexParam* vp = (struct DCVertexParam *)cvec_push_back(&(display_list->vertex_parameters));
                        vp->kind = format;
                        memcpy(vp->data, &this->holly.ta.cmd_buffer[0], vertex_parameter_size(format));
                    }
                }

                if (cmd.end_of_strip == 1) {
                    struct DCVertexStrip* vs = cvec_push_back(&display_list->vertex_strips);
                    vs->user_clip.valid = 0;
                    memcpy(&vs->polygon, &this->holly.ta.cur_poly, sizeof(struct DCPoly));
                    if (this->holly.ta.user_tile_clip.valid && (this->holly.ta.user_tile_clip.user_clip_usage != 0))
                        memcpy(&vs->user_clip, &this->holly.ta.user_tile_clip, sizeof(struct DCUserTileClipInfo));
                    vs->verter_parameter_index = display_list->next_first_vertex_parameters_index;
                    vs->verter_parameter_count = cvec_len(&display_list->vertex_parameters) - display_list->next_first_vertex_parameters_index;
                    display_list->next_first_vertex_parameters_index = cvec_len(&display_list->vertex_parameters);

                }
            }
            ///printf("\nXYZ: %f %f %f", counter, y, z);
            break;
    }

    this->holly.ta.cmd_buffer_index = 0;
}

void holly_TA_FIFO_load(struct DC* this, u32 src_addr, u32 tx_len, void* src)
{
    u32 bytes_tx = 0;
    for (u32 i = 0; i < tx_len; i+= 8) {
        memcpy(&this->holly.ta.cmd_buffer[this->holly.ta.cmd_buffer_index], (src+src_addr+i), tx_len);
        this->holly.ta.cmd_buffer_index += 8;
        printf("\nSZ: %d", this->holly.ta.cmd_buffer_index);
        bytes_tx += 8;
        holly_TA_cmd(this);
    }
    if (bytes_tx < tx_len) {
        printf("\nREVISIT THIS FOOL");
        printf("\nREVISIT THIS FOOL");
        printf("\nREVISIT THIS FOOL");
        printf("\nREVISIT THIS FOOL");
        printf("\nREVISIT THIS FOOL");
        printf("\nREVISIT THIS FOOL");
    }
}

void dump_RAM_to_console(u32 print_addr, void *src, u32 len)
{
    u8* ptr = src;
    // 16 bytes per line
    u32 bytes_printed = 0;
    for (u32 laddr = 0; laddr < len; laddr += 16) {
        printf("\n%08X   ", print_addr+bytes_printed);
        for (u32 i = 0; i < 16; i++) {
            printf("%02X ", (u32)*ptr);
            ptr++;
            bytes_printed++;
            if (bytes_printed >= len) return;
        }
    }
}

void holly_TA_FIFO_DMA(struct DC* this, u32 src_addr, u32 tx_len, void *src, u32 src_len)
{
    if (tx_len == 0) {
        printf("\nHOLLY TA DMA TRANSFER SIZE 0!?");
        return;
    }
    /*if ((src_addr+tx_len) >= src_len) {
        printf(DBGC_RED "\nTOO LONG DMA TRANSFER CH2 %08x" DBGC_RST, src_addr);
        return;
    }*/
    printf("\nSRCADDR:%08x LEN:%d", src_addr, tx_len);
    dump_RAM_to_console(src_addr, this->RAM + (src_addr & 0xFFFFFF), tx_len);
    //holly_TA_FIFO_load(this, src_addr & 0xFFFFFF, tx_len, this->RAM);
    this->sh4.regs.DMAC_SAR2 = src_addr + tx_len;
    this->sh4.regs.DMAC_CHCR2.u &= 0xFFFFFFFE;
    this->sh4.regs.DMAC_DMATCR2 = 0x00000000;

    this->io.SB_C2DST = 0x00000000;
    this->io.SB_C2DLEN = 0x00000000;

    holly_raise_interrupt(this, hirq_ch2_dma, 200);
}

void holly_reset(struct DC* this)
{
    this->holly.VO_BORDER_COL.u = 0;
    this->holly.SPG_VBLANK_INT.u = 0;
    this->holly.SPG_VBLANK_INT.vblank_out_line = 0x150;
    this->holly.SPG_VBLANK_INT.vblank_in_line = 0x104;
    this->holly.SPG_LOAD.vcount = 400; // TODO: not correct but necessary to avoid divide by 0 in simple scheduler_t
}


static void DCDisplayList_init(struct DCDisplayList* this)
{
    this->valid = 0;
    this->next_first_vertex_parameters_index = 0;
    cvec_init(&this->vertex_parameters, sizeof(struct DCVertexParam), 64);
    cvec_init(&this->vertex_strips, sizeof(struct DCVertexStrip), 64);
}

static void DCDisplayList_delete(struct DCDisplayList* this)
{
    this->valid = 0;
    cvec_delete(&this->vertex_strips);
    cvec_delete(&this->vertex_parameters);
}

void holly_init(struct DC* this)
{
    cvec_init(&this->holly.ta.opaque_modifier_volumes, sizeof(struct DCModifierVolume), 64);
    cvec_init(&this->holly.ta.translucent_modifier_volumes, sizeof(struct DCModifierVolume), 64);
    cvec_init(&this->holly.ta.volume_triangles, sizeof(struct DCModifierVolumeParameter), 64);
    for (u32 i = 0; i < 5; i++) {
        DCDisplayList_init(&this->holly.ta.display_lists[i]);
    }
}

void holly_delete(struct DC* this)
{
    cvec_delete(&this->holly.ta.translucent_modifier_volumes);
    cvec_delete(&this->holly.ta.opaque_modifier_volumes);
    cvec_delete(&this->holly.ta.volume_triangles);
    for (u32 i = 0; i < 5; i++) {
        DCDisplayList_delete(&this->holly.ta.display_lists[i]);
    }
}