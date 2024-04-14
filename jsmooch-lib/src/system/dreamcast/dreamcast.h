//
// Created by Dave on 2/11/2024.
//

#ifndef JSMOOCH_EMUS_DREAMCAST_H
#define JSMOOCH_EMUS_DREAMCAST_H

#include "helpers/buf.h"
#include "helpers/sys_interface.h"
#include "helpers/scheduler.h"
#include "helpers/cvec.h"

#include "component/cpu/sh4/sh4_interpreter_opcodes.h"
#include "component/cpu/sh4/sh4_interpreter.h"
#include "dc_mem.h"
#include "spi.h"
#include "gdi.h"
#include "maple.h"
#include "controller.h"
#include "holly.h"
#include "triangle.h"

#define DC_CYCLES_PER_SEC 200000000

#define DC_INT_VBLANK_IN 0x08
#define DC_INT_VBLANK_OUT 0x10
#define DC_INT_GDROM   0x

enum DC_MEM_SIZE {
    DC8 = 1,
    DC16 = 2,
    DC32 = 4,
    DC64 = 8
};

enum HOLLY_PCW_listtype {
    HPL_opaque = 0,
    HPL_opaque_mv = 1,
    HPL_translucent = 2,
    HPL_translucent_mv = 3,
    HPL_punchthru = 4,
    HPL_none = 10
};

enum HOLLY_PCW_paratype {
    ctrl_end_of_list = 0,
    ctrl_user_tile_clip = 1,
    ctrl_object_list_set = 2,
    ctrl_reserved = 3,
    global_polygon_or_modifier_volume = 4,
    global_sprite = 5,
    global_reserved = 6,
    vertex_parameter = 7
};


void DC_new(struct jsm_system* system, struct JSM_IOmap *iomap);
void DC_delete(struct jsm_system* system);

#define HOLLY_VRAM_SIZE 8*1024*1024

struct DCDisplayList {
    u32 valid;
    struct cvec vertex_strips; //std.ArrayList(VertexStrip) = undefined,
    struct cvec vertex_parameters; //std.ArrayList(VertexParameter) = undefined,
    u32 next_first_vertex_parameters_index;
};

struct DC {
    struct SH4 sh4;
    struct SH4_memaccess_t sh4mem;

    struct DC_controller c1;

    u8 RAM[16 * 1024 * 1024];
    u8 VRAM[HOLLY_VRAM_SIZE];

    struct buf BIOS;
    struct buf ROM;

    struct scheduler_t scheduler;

    struct {
        struct buf buf;
    } flash;

    struct {
        u32 broadcast; // 0 -> NTSC, 1 -> PAL, 2 -> PAL/M, 3 -> PAL/N, 4 -> default
        u32 language; // 0 -> JP, 1 -> EN, 2 -> DE, 3 -> FR, 4 -> SP, 5 -> IT, 6 -> default
        u32 region; // 0 -> JP, 1 -> USA, 2 -> EU, 3 -> default
    } settings;

    struct { // io
#include "generated/io_decls.h"
    } io;

    struct {
#include "generated/g2_decls.h"
    } g2;

    struct {
        u32 ARMRST;
        u8 mem[0x8000];
        u8 wave_mem[2*1024*1024];
    } aica;

    struct {
        u32 frame_cycle;
        u64 frame_start_cycle;
        u32 cycles_per_frame;
        u32 cycles_per_line;
        u32 in_vblank;

        struct {
            u32 vblank_in_start;
            u32 vblank_in_end;
            u32 vblank_out_start;
            u32 vblank_out_end;

            u32 vblank_in_yet;
            u32 vblank_out_yet;

        } interrupt;
    } clock;

    struct {
#include "generated/holly_decls.h"

        float FPU_CULL_VAL;
        float FPU_PERP_VAL;
        float ISP_BACKGND_D;
        u32 FOG_TABLE[128];
        u32 last_used_buffer;
        u32 cur_output_num;
        u32 *cur_output;
        u32 *out_buffer[2];

        u64 master_frame;

        struct {
            enum HOLLY_PCW_listtype list_type;
            struct DCPoly cur_poly;
            struct DCModifierVolume cur_volume;
            struct DCUserTileClipInfo user_tile_clip;

            struct cvec opaque_modifier_volumes;
            struct cvec translucent_modifier_volumes;
            struct cvec volume_triangles;
            u32 tri_type;

            u32 cmd_buffer_index;
            u8 cmd_buffer[16]; // Up to 16 bytes per command

            struct DCDisplayList display_lists[5];
        } ta;
    } holly;

    struct {
#include "generated/g1_decls.h"
    } g1;

    struct {
        union {
            struct {
                u32 : 1;
                u32 nIEN: 1;
            };
            u32 u;
        } device_control;

        union {
            struct {
                u32 CHECK : 1; // 1 = error occurred
                u32 NU: 1; // RESERVED
                u32 CORR: 1; // 1 = correctable error
                u32 DRQ: 1; // 1 when data transfer with host is possible (waiting?)
                u32 DSC : 1; // 1 means seek processing complete
                u32 DF: 1; // drive fault info
                u32 DRDY: 1; // 1 when response to ATA command is possible (waiting?)
                u32 BSY: 1; // 1 when command is accepted
            };
            u8 u;
        } device_status;

        union {
            struct {
                u8 lo;
                u8 hi;
            };
            u16 u;
        } byte_count;

        union {
            struct {
                u32 mode_value: 3;
                u32 transfer_mode: 5;
            };
            u32 u;
        } sector_count;

        u32 ata_cmd;

        union {
            struct {
                u32 status : 4;
                u32 disc_format : 4;
            };
            u32 u;
        } sector_number;

        union {
            struct {
                u32 ILI: 1;
                u32 EOMF: 1;
                u32 ABORT: 1;
                u32 MCR: 1;
                u32 ERROR: 4;
            };
            u32 u;
        } error;

        u32 sns_asc;
        u32 sns_ascq;
        u32 sns_key;

        u32 features;
        u32 cmd;

        u32 state;

        struct {
            u32 playing;
        } cdda;

        struct
        {
            union
            {
                struct
                {
                    u32 CoD:1; //Bit 0 (CoD) : "0" indicates data and "1" indicates a command.
                    u32 IO:1;  //Bit 1 (IO)  : "1" indicates transfer from device to host, and "0" from host to device.
                    u32 any :6;//not used
                };
                u8 u;
            };
        } interrupt_reason;

        struct SPI_packet_cmd packet_cmd;

        struct GDROM_PIOBUF {
            u32 next_state;
            u32 index;
            u32 size;
            u16 data[0x8000]; //64 kb
        } pio_buff;

        struct GDI_image gdi;
    } gdrom;

    struct {
        u32 SB_FFST;
        u32 SB_FFST_rc;
    } sb;

    struct {
#include "generated/maple_decls.h"
        u32 vblank_repeat_trigger;

        struct MAPLE_port ports[4];
    } maple;

    struct {
        void *rptr[0x40];
        void *wptr[0x40];
        u64 (*read[0x40])(void*, u32, enum DC_MEM_SIZE, u32*);
        void (*write[0x40])(void*, u32, u64, enum DC_MEM_SIZE, u32*);
    } mem;

};

enum holly_interrupt_hi {
    holly_nrm = 0,
    holly_ext = 0x100,
    holly_err = 0x200
};

enum holly_interrupt_masks {
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

void DC_mem_init(struct DC* this);

#endif //JSMOOCH_EMUS_DREAMCAST_H
