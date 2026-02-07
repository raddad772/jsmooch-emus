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
        u8 MODE_sector_size;
    } latch{};

    union {
        struct {
            u8 error : 1; // 1=invalid cmd/parameters
            u8 motor_fullspeed : 1; // 1=motor full speed
            u8 seek_error : 1; // 1=seek error
            u8 id_error : 1; // 1=getID denied
            u8 shell_open : 1; // is/was open
            u8 read : 1; // reading data sectors
            u8 seek : 1;
            u8 play : 1;
        };
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
            u8 CDDA : 1; // 1=allow CD-DA sectors
            u8 auto_pause : 1; // 1=auto pause end of track
            u8 report : 1; // 1=enable report interrupts for audio
            u8 xa_filter : 1; // 1=process only XA-ADPCM sectors that match SetFilter
            u8 ignore_bit : 1; // 1=ignore sectroc size and setloc positions
            u8 sector_size : 1; // 0=800h data_only, 1=924h whole sector except sync bytes
            u8 xa_adpcm : 1; // 1= send XA-ADPCM sectors to SPU Audio Input
            u8 speed : 1; // 0=normal/1x, 1=double/2x
        };
        u8 u{};
    } MODE{};

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
enum CDHEAD_MODE {
    HM_AUDIO, HM_DATA
};

struct CDROM {
    explicit CDROM(scheduler_t *scheduler_in) : scheduler(scheduler_in) {}
    scheduler_t *scheduler;
    void mainbus_write(u32 addr, u32 val, u8 sz);
    u32 mainbus_read(u32 addr, u8 sz, bool has_effect);
    void update_IRQs();
    void insert_disc(multi_file_set &mfs);

    void cmd_start(u64 key, u64 clock);
    void cmd_finish(u64 key, u64 clock);
    void cmd_step3(u64 key, u64 clock);
    void sch_read(u64 key, u64 clock);
    void read_toc();
    void do_seek();

    void *set_irq_ptr{};
    void (*set_irq_lvl)(void*, u32){};

    cvec_ptr<physical_io_device> pio_ptr{};
    JSM_DISC_DRIVE *dd{};

    CDROM_IO io{};

    struct {
        // TODO: this
        u8 step{};

        u32 still_sched{};
        u64 sched_id{};
    } CMD{};

    struct {
        u32 still_sched{};
        u64 sched_id{};
    } read{};

    struct {
        struct {
            u8 file{}, channel{};
        } filter{};
    } ADPCM{};

    struct {
        u8 amm{}, ass{}, asect{};
        u8 session{};
    } seek;

    struct {
        CDHEAD_MODE mode;
        // can be seeking,
        // can be reading
        // can be data or audio mode

        u32 sector{}, session{};
    } head;
    struct {
        u32 num_sessions;
    } disk;

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
    void schedule_finish(u64 clock);
    void schedule_read(u64 clock);
    void schedule_step_3(u64 clock);
    void schedule_seek_finish(u64 clock);;
    void cancel_CMD();
    void reset_decoder();
    void finish_CMD(bool do_stat_irq, u32 irq_num);
    void queue_interrupt(u32 lvl);
    void result(u32 val);
    void stat_irq();
    void cmd_setloc();
    void cmd_play();
    void cmd_forward();
    void cmd_backward();
    void cmd_readn(u64 clock);
    void cmd_reads(u64 clock);
    void cmd_motor_on(u64 clock);
    void cmd_motor_on_finish();
    void cmd_stop(u64 clock);
    void cmd_pause(u64 clock);
    void cmd_init(u64 clock);
    void cmd_setmode();
    void cmd_reset();
    void cmd_seekl(u64 clock);
    void cmd_seekp(u64 clock);
    void cmd_set_session(u64 clock);
    void cmd_set_session_finish(u64 clock);
    void do_cmd_read(u64 clock);
    void do_cmd_read_step2(u64 clock);
};
}