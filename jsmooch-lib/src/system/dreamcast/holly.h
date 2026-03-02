//
// Created by Dave on 2/16/2024.
//
#pragma once

namespace DC {
    struct core;
}

namespace DC::HOLLY {


enum interrupthi {
    holly_nrm = 0,
    holly_ext = 0x100,
    holly_err = 0x200
};

enum interruptmasks {
    hirq_vblank_in = 4,
    hirq_vblank_out = 5,

    hirq_YUV_DMA = holly_nrm | 6,
    hirq_opaque_list = holly_nrm | 7,
    hirq_opaque_modifier_list = holly_nrm | 8,

    hirq_translucent_list = holly_nrm | 9,
    hirq_translucent_modifier_list = holly_nrm | 10,

    hirq_maple_dma = holly_nrm | 12,
    hirq_ch2_dma = holly_nrm | 19,
    hirq_punchthru = holly_nrm | 21,
    hirq_gdrom_cmd = holly_ext | 0
};
    enum PCW_listtype {
        HPL_opaque = 0,
        HPL_opaque_mv = 1,
        HPL_translucent = 2,
        HPL_translucent_mv = 3,
        HPL_punchthru = 4,
        HPL_none = 10
    };

    enum PCW_paratype {
        ctrl_end_of_list = 0,
        ctrl_user_tile_clip = 1,
        ctrl_object_list_set = 2,
        ctrl_reserved = 3,
        global_polygon_or_modifier_volume = 4,
        global_sprite = 5,
        global_reserved = 6,
        vertex_parameter = 7
    };

#define HOLLY_VRAM_SIZE 8*1024*1024


union PCW {
    struct {
        u32 uv_is_16bit: 1;
        u32 gouraud: 1;
        u32 offset: 1;
        u32 texture: 1;
        u32 col_type: 2;
        u32 volume: 1;
        u32 shadow: 1;
        u32 : 8;
        u32 user_clip: 2;
        u32 strip_len: 2;
        u32 : 3;
        u32 group_en: 1;
        u32 list_type: 3;
        u32 : 1;
        u32 end_of_strip: 1;
        u32 para_type: 3;
    };
    u32 u{};
};

union ISP_TSP_WORD {
    struct {
        u32 : 20;
        u32 dcalc_ctrl : 1;
        u32 cache_bypass : 1;
        u32 uv_16bit : 1;
        u32 gouraud : 1;
        u32 offset : 1;
        u32 texture : 1;
        u32 z_write_disable : 1;
        u32 culling_mode : 2;
        u32 depth_compare_mode : 3;
    };
    u32 u{};
};

struct  core {
#include "generated/holly_decls.h"
    explicit core(DC::core *parent) : bus(parent) { ta.cmd_buffer.allocate(2*1024*1024); }
    u64 read(u32 addr, bool* success);
    void write(u32 addr, u32 val, bool* success);
    void reset();
    void soft_reset();
    void recalc_frame_timing();
    void TA_list_init();
    void recalc_interrupts();
    void TA_FIFO_DMA(u32 src_addr, u32 tx_len, void *src, u32 src_len);
    void raise_interrupt(interruptmasks irq_num, i64 delay);
    void eval_interrupt(interruptmasks irq_num, u32 is_true);
    void lower_interrupt(interruptmasks irq_num);
    void dump_RAM_to_console(u32 print_addr, void *src, u32 len);
    void vblank_in();
    void vblank_out();
    void TA_cmd();
    void TA_FIFO_load(u32 src_addr, u32 tx_len, void* src);

    float FPU_CULL_VAL{};
    float FPU_PERP_VAL{};
    float ISP_BACKGND_D{};
    u32 FOG_TABLE[128]{};
    u32 *cur_output{};
    DC::core *bus;

    u64 master_frame{};

    struct {
        PCW_listtype list_type{};

        u32 tri_type{};

        u32 cmd_buffer_index{};
        buf cmd_buffer{};
    } ta{};

    JSM_DISPLAY* display{};
    cvec_ptr<physical_io_device> display_ptr{};
private:
    u32 get_frame_cycle();
    u32 get_SPG_line();
};

}