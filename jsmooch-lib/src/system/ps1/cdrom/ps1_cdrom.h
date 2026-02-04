//
// Created by . on 3/6/25.
//

#pragma once
#include "helpers/scheduler.h"
#include "helpers/int.h"
#include "helpers/physical_io.h"
#include "helpers/cvec.h"

namespace PS1 {

struct CDROM;

struct CD_FIFO {
    void reset();
    void push(u32 val);
    u8 pop();
    u8 entries[16]{};
    u8 head{}, tail{}, len{};
};

struct CD_DATA_BUF {
    u8 values[0x940]{};
    u32 pos{};
    u32 len{};
    u32 read_byte() {
        if (pos >= len) return 0;
        u8 v = values[pos];
        pos++;
        return v;
    }
    void clear() { len = 0; pos = 0; }
};

struct CDROM_IO {
    CD_FIFO PARAMETER{};
    CD_FIFO RESULT{};
    CD_DATA_BUF RDDATA{};
    CD_FIFO interrupts{};

    u8 L_L{}, L_R{}, R_L{}, R_R{};
    struct {
        u8 L_L{}, L_R{}, R_L{}, R_R{};
    } latch{};

    union {
        u8 error : 1; // 1=invalid cmd/parameters
        u8 motor_fullspeed : 1; // 1=motor full speed
        u8 seek_error : 1; // 1=seek error
        u8 id_error : 1; // 1=getID denied
        u8 shell_open : 1; // is/was open
        u8 read : 1; // reading data sectors
        u8 u;
    } stat;

    union {
        struct {
            u8 RA : 2; // current register
            u8 ADPBUSY : 1; // 1=playing xa-adpcm
            u8 PRMEMPT : 1; // 1=parameter FIFO empty
            u8 PRMWRDY : 1; // 1=parameter FIFO not full
            u8 RSLRRDY : 1; // 1=result FIFO not empty
            u8 DRQSTS : 1; // 1=one or more RDDATA or WRDATA pending
            u8 BUSYSTS : 1; // 1=HC05 busy acknowledging command
        };
        u8 u{};
    } HSTS{};

    union {
        struct {
            u8 INTSTS : 3 {0}; // Interrupt #
            u8 BFEMPT : 1 {0}; // 1=XA-ADPCM decoder ran out of sectors to play
            u8 BFWRDY : 1 {0}; // 1=XA-ADPCM decoder is ready for next sector
            u8 res : 3 {0b111};
        };
        u8 u;
    } HINTSTS{};

    union {
        struct {
            u8 ENINT : 3{0}; // enable interrupt on these pins
            u8 ENBFEMPT : 1{0}; // enable interrupt on BFEMPT
            u8 ENBFWRDY : 1{0}; // enable interrupt on BFWRDY
            u8 res : 3 {0b111}; // always 1 when read
        };
        u8 u;
    } HINTMSK{};

    union {
        struct {
            u8 SM : 1; // 0=mono 1=stereo
            u8 _res0 : 1;
            u8 FS : 1; // Samplerate. 0 = 37800, 1=18900
            u8 _res1 : 1;
            u8 BITLNGTH : 1; // 0=4bit 1=8bit bits per sample
            u8 _res2 : 1;
            u8 EMPHASIS : 1; // Emphasis filter 0=off
            u8 _res3 : 1;
        };
        u8 u;
    } CI;

    u8 CMD{};
};

struct CDROM {
    explicit CDROM(scheduler_t *scheduler_in) : scheduler(scheduler_in) {}
    scheduler_t *scheduler;
    void write(u32 addr, u32 val, u8 sz);
    u32 read(u32 addr, u8 sz, bool has_effect);
    void update_IRQs();

    void *set_irq_ptr{};
    void (*set_irq_lvl)(void*, u32){};

    CDROM_IO io{};

    struct {
        // TODO: this
        u8 step{};

        u32 still_sched{};
        u64 sched_id{};
    } CMD{};


private:

    u32 read_01(u8 sz, bool has_effect);
    u32 read_02(u8 sz, bool has_effect);
    u32 read_03(u8 sz, bool has_effect);
    void write_01(u32 val, u8 sz);
    void write_02(u32 val, u8 sz);
    void write_03(u32 val, u8 sz);
    void write_CMD(u32 val);
    void ack_IRQ();
    void reset(); // TODO: CALL THIS
    void recalc_HSTS();
    void schedule_CMD();
    void cancel_CMD();
    void reset_decoder();
    void finish_CMD();
    void queue_interrupt(u32 lvl);
    void result(u32 val);

    void cmd00();
    void cmd03();

};
}