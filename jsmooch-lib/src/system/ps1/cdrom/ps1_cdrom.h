//
// Created by . on 3/6/25.
//

#pragma once
#include "helpers/scheduler.h"
#include "helpers/int.h"
#include "helpers/physical_io.h"
#include "helpers/cvec.h"
#include "helpers/cdrom_formats.h"

namespace PS1 {

struct CDROM;

struct CD_FIFO {
    void reset();
    void push(u32 val);
    u8 pop();
    u8 entries[16]{};
    u8 head{}, tail{}, len{};
};

struct CD_FIFO_RESULTS_BUF {
    u8 data[16]{};
    u32 tail{};
    u32 head{};
    u32 len{};
    void reset() { len = 0; head = tail = 0; }
    void push(u8 val) {
        if (len >= 16) {
            printf("\n(CD FIFO RESULTS BUF) attempt to push to full!");
            return;
        }
        data[tail] = val;
        tail = (tail + 1) & 15;
        len++;
    }
    u8 pop() {
        if (len == 0) return data[head];
        u8 v = data[head];
        head = (head + 1) & 15;
        len--;
        return v;
    }
    void copy(CD_FIFO_RESULTS_BUF &other) {
        memcpy(data, other.data, sizeof(other.data));
        tail = other.tail;
        head = other.head;
        len = other.len;
    }
};

struct CD_FIFO_IRQ_ENTRY {
    u8 val{};
    CD_FIFO_RESULTS_BUF results{};
};

struct CD_FIFO_IRQ {
    void reset();
    void push_irq(u32 val);
    u8 pop_irq();

    void push_result(u8 val);
    u8 pop_result();
    CD_FIFO_IRQ_ENTRY entries[16]{};
    u8 head{}, tail{}, len{};

    u32 cur_IRQ_val{};
    CD_FIFO_RESULTS_BUF in_results{};
    CD_FIFO_RESULTS_BUF out_results{};
};


struct CD_DATA_BUF {
    u8 *ptr;
    u32 pos{};
    i32 len{};
    u32 read_byte() {
        u8 v;
        if (pos >= len) v = ptr[pos-1];
        else {
            v = ptr[pos];
            pos++;
        }
        return v;
    }
    void clear() { ptr = nullptr; len = 0; pos = 0; }
};

struct CDROM_IO {
    CD_FIFO PARAMETER{};
    CD_DATA_BUF RDDATA{};
    CD_FIFO_IRQ interrupts{};

    i32 L_L{}, L_R{}, R_L{}, R_R{};
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
            u8 play : 1; // palying
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
            u8 _res : 5;
            u8 SMEN : 1;
            u8 BFWR : 1;
            u8 BFRD : 1; // 1= allow FIFO reads, 0 = return 0's
        };
        u8 u{};
    } HCHPCTL{};

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

struct SECTOR_BUFFER {
    u32 head{}, tail{}, len{};
    u8 bufs[8][0x930];
};

struct core;

struct CDROM {
    explicit CDROM(core *parent) : bus(parent) {}
    core *bus;
    void get_CD_audio(i16 &left, i16 &right);
    void mainbus_write(u32 addr, u32 val, u8 sz);
    u32 mainbus_read(u32 addr, u8 sz, bool has_effect);
    void update_IRQs();
    void insert_disc(multi_file_set &mfs);
    void remove_disc();
    void open_drive();
    void close_drive();

    struct {
        u32 spinning_up{};
    } motor{};

    void cmd_start(u64 key, u64 clock);
    void cmd_finish(u64 key, u64 clock);
    void cmd_step3(u64 key, u64 clock);
    void sch_read(u64 key, u64 clock);
    void read_toc();
    void do_seek();

    void queue_sector_readback();

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
        CDHEAD_MODE mode{};
        // can be seeking,
        // can be reading
        // can be data or audio mode

        u32 sector{}, session{};
        u32 sample_index{}; // 0/2352
        bool muted{false};
    } head{};

    SECTOR_BUFFER sector_buf{};

    struct {
        u8 subcmd{};
        u32 counterlo{}, counterhi{};
    } test{};

    CDROM_DISC data{};
    struct {
        u32 num_sessions{false};
        bool inserted{false};
    } disk{};

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
    void result_string(const char *s);
    void stat_irq();
    void cmd_setloc();
    void cmd_test();
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
    void cmd_getid(u64 clock);
    void cmd_getid_finish();
    void cmd_set_session(u64 clock);
    void cmd_set_session_finish(u64 clock);
    void do_cmd_read(u64 clock);
    void read_sector();
    void cmd_demute();
    void cmd_gettn();
    void cmd_gettd();
    void queue_sector_RDDATA();
};
}