//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_NDS_BUS_H
#define JSMOOCH_EMUS_NDS_BUS_H

//#define TRACE

#include "helpers/scheduler.h"
#include "nds_clock.h"
#include "nds_ppu.h"
#include "nds_controller.h"
#include "cart/nds_cart.h"
#include "nds_ipc.h"
#include "system/nds/3d/nds_ge.h"
#include "system/nds/3d/nds_re.h"
#include "nds_apu.h"

#include "component/cpu/arm7tdmi/arm7tdmi.h"
#include "component/cpu/arm946es/arm946es.h"

struct NDS;
typedef u32 (*NDS_rdfunc)(struct NDS *, u32 addr, u32 sz, u32 access, u32 has_effect);
typedef void (*NDS_wrfunc)(struct NDS *, u32 addr, u32 sz, u32 access, u32 val);

#define NDSVRAMSHIFT(nda) (((nda) & 0xFFFFFF) >> 14)
#define NDSVRAMMASK 0x3FF

struct NDS {
    struct ARM7TDMI arm7;
    struct ARM946ES arm9;
    struct NDS_clock clock;
    struct NDS_PPU ppu;
    struct NDS_GE ge;
    struct NDS_RE re;
    struct NDS_APU apu;
    u32 arm9_ins, arm7_ins;
    struct NDS_controller controller;

    struct scheduler_t scheduler;

    struct {
        u64 current_transaction;
        u64 current_shift; // 1
    } waitstates;

    struct {
        struct { // Only bits 27-24 are needed to distinguish valid endpoints, mostly.
            NDS_rdfunc read[16];
            NDS_wrfunc write[16];
        } rw[2]; // ARM7, ARM9 maps...
        u8 RAM[0x400000]; // 4MB RAM
        u8 WRAM_share[32 * 1024];      // 32KB WRAM mappable
        u8 WRAM_arm7[64 * 1024]; // 64KB of WRAM for ARM7 only
        struct {
            u8 data[16 * 1024];
            u8 code[32 * 1024];
        } tcm;
        u8 oam[2048];
        u8 palette[2048];
        u8 internal_3d[248 * 1024];
        u8 wifi[8 * 1024];
        u8 bios7[16384];
        u8 bios9[4096];
        u8 firmware[256 * 1024];
        struct {
            struct {
                u32 base, mask, disabled, val;
            } RAM7, RAM9;
        } io;

        struct NDS_VRAM {
            u8 data[656 * 1024];
            struct {
                struct {
                    u32 mst, ofs, enable;
                } bank[9];
            } io;
            struct {
                u8 *arm9[0x400]; // 16KB banks from 06000000 to 06FFFFFF
                u8 *arm7[2];    // 2 128KB banks at 06000000. addr < 06400000, slot[0] or [1] & 128KB
            } map;
        } vram;

    } mem;

    struct {
        struct {
            u8 filldata[16];
        } dma;

        u8 POSTFLG;

        struct {
            u32 arm7, arm9, bios, dma;
        } open_bus;

        struct {
            struct NDS_CPU_FIFO to_arm7, to_arm9;

            struct {
                u32 irq_on_send_fifo_empty;
                u32 irq_on_recv_fifo_not_empty;
                u32 error;
                u32 fifo_enable;
            } arm7, arm9;
            struct {
                u32 dinput;
                u32 doutput;
                u32 enable_irq_from_remote;

            } arm7sync, arm9sync;
        } ipc;


        struct {
            u32 BIOSPROT;
            u32 EXMEM;
            u32 IF, IE, IME, IE_val;
            struct {
                u32 buttons;
                u32 enable, condition;
            } button_irq;
            u32 halted;
            u32 POSTFLG;
        } arm7;

        struct {
            u32 EXMEM;
            u32 IF, IE, IME;
            struct {
                u32 buttons;
                u32 enable, condition;
            } button_irq;
            u32 POSTFLG;
        } arm9;

        struct {
            u32 gba_slot;
            u32 nds_slot;
            u32 main_memory;
        } rights;

        struct {
            u64 busy_until;
            u32 mode;
            u32 by_zero;
            u32 needs_calc;

            union NDSreg64 {
                u8 data[8];
                u32 data32[2];
                u64 u;
           } numer, denom, result, remainder;
            // numer & demom are r/w, result and remainder are R-only
        } div;

        struct {
            u64 busy_until;
            u32 mode;
            u32 needs_calc;

            union NDSreg32 {
                u8 data[4];
                u32 u;
            } result; // r-only
            union NDSreg64 param; // r/w
        } sqrt;

        struct {
            u64 timestamp;
            u16 data;

            u8 input;
            u32 input_bit, input_pos;
            u8 output[8];
            u32 output_bit, output_pos;
            u32 cmd;
            u32 status_reg[2];
            u32 date_time[7];
            u32 alarm1[3], alarm2[3];

            u64 divider;

            u64 minute_count;
            u32 irq_flag;
            u32 clock_adjust;
            u32 free_register;
            u32 alarm_date1[3];
            u32 alarm_date2[3];
            u32 FOUT1, FOUT2;
            u64 sch_id;
        } rtc;

        struct {
            u32 rcnt;
        } sio;

        struct {
            u32 lcd_enable;

            u32 speakers;
            u32 wifi;
            u32 wifi_waitcnt;

        } powcnt;

        u32 sio_data;
    } io;

    struct {
        union {
            struct {
                // byte 0
                u32 baudrate : 2;
                u32 _r : 5;
                u32 busy : 1;

                // byte 1
                u32 device : 2;
                u32 transfer_sz : 1;
                u32 chipselect_hold : 1;
                u32 _r2 : 2;
                u32 irq_enable : 1;
                u32 bus_enable : 1;
            };
            u32 u;
        } cnt;
        u32 enable;
        u64 busy_until;
        u32 input, output;
        u32 chipsel;

        struct {
            u32 hold;
            u32 index, pos;
            u32 dir;
            u8 regs[4];
        } pwm;

        struct {
            u32 write_enable;
            u32 status;

            u32 input[8];
            u32 output[8];
            struct {
                u32 pos;
                u32 addr;
                u32 cur;
            } cmd;

            u32 num_params;

            u32 hold;
        } firmware;

        struct NDS_SPI_TOUCHSCREEN {
            union {
                struct {
                    u8 penirq : 1;
                    u8 modemsb: 1;
                    u8 ref_select : 1;
                    u8 conversion_mode : 1;
                    u8 chan_select : 3;
                    u8 start : 1;
                };
                u8 u;
            } cnt;
            u32 result;
            u32 pos;
            u32 touch_x, touch_y;
            u32 hold;

            struct physical_io_device *pio;

            i32 adc_x_top_left, adc_y_top_left;
            i32 adc_x_delta, adc_y_delta;
            i32 screen_x_top_left, screen_y_top_left;
            i32 screen_x_delta, screen_y_delta;

        } touchscr;

        u64 irq_id;
        u64 irq_scheduled;

        u32 hold;
    } spi;

    struct {
        struct cvec* IOs;
        u32 described_inputs;
        i64 cycles_left;
    } jsm;

    struct NDS_DMA_ch {
        u32 num;
        struct {
            u32 src_addr; // 28 bits
            u32 dest_addr; // 28 bits
            u32 word_count; // 14 bits on ch0-2, 16bit on ch3

            u32 dest_addr_ctrl;
            u32 src_addr_ctrl;
            u32 repeat;
            u32 transfer_size;

            u32 start_timing;
            u32 irq_on_end;
            u32 enable;
            u32 open_bus;
        } io;

        struct {
            u32 started;
            u32 word_count;
            u32 word_mask;
            u32 src_addr;
            u32 dest_addr;
            u32 src_access, dest_access;
            u32 sz;
            u32 first_run;
            u32 is_sound;
            i32 chunks;
            // for mode GE_FIFO
        } op;
        u32 run_counter;
    } dma7[4], dma9[4];

    struct NDS_TIMER {
        struct {
            u32 io;
            u32 mask;
            u32 counter;
        } divider;

        u32 shift;

        u64 enable_at; // cycle # we'll be enabled at
        u64 overflow_at; // cycle # we'll overflow at
        u64 sch_id;
        u32 sch_scheduled_still;
        u32 cascade;
        u16 val_at_stop;
        u32 irq_on_overflow;
        u16 reload;
        u64 reload_ticks;
    } timer7[4], timer9[4];

    struct {
        double master_cycles_per_audio_sample;
        double next_sample_cycle;
        struct audiobuf *buf;
    } audio;

    struct {
        struct NDS_DBG_eng {
            struct NDS_DBG_line {
                struct NDS_DBG_line_bg {
                    union NDS_PX buf[256];
                    u32 hscroll, vscroll;
                    i32 hpos, vpos;
                    i32 x_lerp, y_lerp;
                    i32 pa, pb, pc, pd;
                    u32 reset_x, reset_y;
                    u32 htiles, vtiles;
                    u32 display_overflow;
                    u32 screen_base_block, character_base_block;
                    u32 priority;
                    u32 bpp8;
                } bg[4];
                union NDS_PX sprite_buf[256];
                u16 dispcap_px[256];
                u32 bg_mode;
            } line[192];
        } eng[2];
        struct NDS_DBG_tilemap_line_bg {
            u8 lines[1024 * 128]; // 4, 128*8 1-bit "was rendered or not" values
        } bg_scrolls[4];
        struct {
            u32 enable;
            char str[257];
        } mgba;
    } dbg_info;

    struct NDS_cart cart;

    DBG_START
        DBG_EVENT_VIEW

        DBG_CPU_REG_START(arm7)
            *R[16]
        DBG_CPU_REG_END(arm7)

        DBG_CPU_REG_START(arm9)
                    *R[16]
        DBG_CPU_REG_END(arm9)

        DBG_IMAGE_VIEWS_START
            MDBG_IMAGE_VIEW(palettes)
            MDBG_IMAGE_VIEW(a_window0)
            MDBG_IMAGE_VIEW(a_window1)
            MDBG_IMAGE_VIEW(a_window2)
            MDBG_IMAGE_VIEW(a_window3)
            MDBG_IMAGE_VIEW(a_bg0)
            MDBG_IMAGE_VIEW(a_bg1)
            MDBG_IMAGE_VIEW(a_bg2)
            MDBG_IMAGE_VIEW(a_bg3)
            /*MDBG_IMAGE_VIEW(a_bg0map)
            MDBG_IMAGE_VIEW(a_bg1map)
            MDBG_IMAGE_VIEW(a_bg2map)
            MDBG_IMAGE_VIEW(a_bg3map)*/
            MDBG_IMAGE_VIEW(a_sprites)
            MDBG_IMAGE_VIEW(a_tiles)
            /*MDBG_IMAGE_VIEW(b_window0)
            MDBG_IMAGE_VIEW(b_window1)
            MDBG_IMAGE_VIEW(b_window2)
            MDBG_IMAGE_VIEW(b_window3)*/
            MDBG_IMAGE_VIEW(b_bg0)
            MDBG_IMAGE_VIEW(b_bg1)
            MDBG_IMAGE_VIEW(b_bg2)
            MDBG_IMAGE_VIEW(b_bg3)
            /*MDBG_IMAGE_VIEW(b_bg0map)
            MDBG_IMAGE_VIEW(b_bg1map)
            MDBG_IMAGE_VIEW(b_bg2map)
            MDBG_IMAGE_VIEW(b_bg3map)*/
            MDBG_IMAGE_VIEW(b_sprites)
            MDBG_IMAGE_VIEW(b_tiles)
            MDBG_IMAGE_VIEW(re_wireframe)
            MDBG_IMAGE_VIEW(re_attr)
            MDBG_IMAGE_VIEW(ppu_info)
            MDBG_IMAGE_VIEW(ppu_layers)
            MDBG_IMAGE_VIEW(dispcap)
            MDBG_IMAGE_VIEW(re_output)
        DBG_IMAGE_VIEWS_END
        DBG_WAVEFORM_START1
            DBG_WAVEFORM_MAIN
            DBG_WAVEFORM_CHANS(6)
        DBG_WAVEFORM_END1
        DBG_LOG_VIEW
    DBG_END

};

/*
    u32 (*read)(void *ptr, u32 addr, u32 sz, u32 access, u32 has_effect);
    void (*write)(void *ptr, u32 addr, u32 sz, u32 access, u32 val);
 */
u32 NDS_mainbus_read7(void *ptr, u32 addr, u32 sz, u32 access, u32 has_effect);
void NDS_mainbus_write7(void *ptr, u32 addr, u32 sz, u32 access, u32 val);
u32 NDS_mainbus_fetchins7(void *ptr, u32 addr, u32 sz, u32 access);

u32 NDS_mainbus_read9(void *ptr, u32 addr, u32 sz, u32 access, u32 has_effect);
void NDS_mainbus_write9(void *ptr, u32 addr, u32 sz, u32 access, u32 val);
u32 NDS_mainbus_fetchins9(void *ptr, u32 addr, u32 sz, u32 access);


void NDS_bus_init(struct NDS *);
void NDS_eval_irqs(struct NDS *);
void NDS_check_dma9_at_hblank(struct NDS *);
void NDS_check_dma7_at_vblank(struct NDS *);
void NDS_check_dma9_at_vblank(struct NDS *);
u32 NDS_open_bus_byte(struct NDS *, u32 addr);
u32 NDS_open_bus(struct NDS *this, u32 addr, u32 sz);
u64 NDS_clock_current7(struct NDS *);
u64 NDS_clock_current9(struct NDS *);
void NDS_bus_reset(struct NDS *);
#endif //JSMOOCH_EMUS_NDS_BUS_H
