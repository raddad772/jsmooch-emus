//
// Created by . on 3/6/25.
//

#pragma once
#include "helpers/int.h"
#include "helpers/physical_io.h"
#include "helpers/cvec.h"
#include "helpers/cdrom_formats.h"
namespace PS1 { struct core; }
namespace PS1::CDROM {

struct FIFO16 {
    void reset();
    void push(u8 val);
    u8 pop();
    u8 entries[16]{};
    u8 head{}, tail{}, len{};
};

struct SECTOR_FIFO {
    u32 pos{}, len{};
    u8 data[0x930]{};
    void push(u8 *ptr, u32 num);
    u32 pop();
    void clear();
};


struct IO {
    SECTOR_FIFO buffered_data{};
    FIFO16 param{};
    FIFO16 response{};

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
            u8 seek : 1; // seeking
            u8 play : 1; // playing
        };
        u8 u;
    } stat;

    union {
        struct {
            u8 RA : 2; // 0-1. current register   1,2
            u8 ADPBUSY : 1; // 2. 1=playing xa-adpcm 4
            u8 PRMEMPT : 1; // 3. 1=parameter FIFO empty 8
            u8 PRMWRDY : 1; // 4. 0x10 1=parameter FIFO not full 0x10
            u8 RSLRRDY : 1; // 5. 0x20 1=result FIFO not empty 0x20
            u8 DRQSTS : 1; // 6. 0x40 1=one or more RDDATA or WRDATA pending 0x40
            u8 BUSYSTS : 1; // 7. 1=HC05 busy acknowledging command 0x80
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
            u8 xa_adpcm : 1; // 1= send XA-ADPCM sectors to SPU Audio Input, 0= don't
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


namespace XA {
enum XA_CH {
    MONO = 0,
    STEREO = 1,
    RESERVED1 = 2,
    RESERVED2 = 3
};

enum XA_SR {
    SR37800=0,
    SR18900=1,
    RRESERVED1 = 2,
    RRESERVED2 = 3
};

enum XA_BPS {
    BITS4 = 0,
    BITS8 = 1,
    RRRESERVED1 = 2,
    RRRESERVED2 = 3
};

struct DECODER {
    struct {
        i16 l[4032]{};
        i16 r[4032]{};
        u32 len{};
    } samples{};

    struct {
        i16 l[32]{}, r[32]{};
        u32 pos{};
    } ringbuf{};
    i32 six{6};

    struct {
        u32 pos{};
        u32 len{};
        i16 l[4704];
        i16 r[4704];
    } out_samples{};
    bool last_block{};
    i16 l_old{}, r_old{}, l_older{}, r_older{};
};
}

struct core {
    explicit core(PS1::core *parent) : bus(parent) {}
    PS1::core *bus;
    void get_CD_audio(i16 &left, i16 &right);
    void mainbus_write(u32 addr, u32 val, u8 sz);
    u32 mainbus_read(u32 addr, u8 sz, bool has_effect);
    void update_IRQs();
    void insert_disc(multi_file_set &mfs);
    void remove_disc();
    void open_drive();
    void close_drive();
    void dbg_irq(u32 num);

    struct {
        u32 spinning_up{};
    } motor{};

    void cmd_start(u64 key, u64 clock);
    void cmd_finish(u64 key, u64 clock);
    void cmd_step3(u64 key, u64 clock);
    void sch_read(u64 key, u64 clock);

    void *set_irq_ptr{};
    void (*set_irq_lvl)(void*, u32){};

    cvec_ptr<physical_io_device> pio_ptr{};
    JSM_DISC_DRIVE *dd{};

    struct IRQSTRUCT {
        struct IRQSRC { bool enable{}; bool flag{}; };
        IRQSRC ready{}, complete{}, ack{}, end{}, error{};

        [[nodiscard]] bool pending() const {
            return ready.flag | complete.flag | ack.flag | end.flag | error.flag;
        }

        [[nodiscard]] bool any_can_fire() const {
            return (ready.flag & ready.enable) | (complete.flag & complete.enable) | (ack.flag & ack.enable) | (end.flag & end.enable) | (error.flag & error.enable);
        }

        struct DEFERREDSTRUCT {
            struct DEFERREDSTRUCTITEM {
                u8 kind{};
                FIFO16 data{};
            } ready{}, complete{}, ack{};
        } deferred{};
    } irq{};

    IO io{};

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

    void latch_seek();
    struct {
        u8 session{};
        struct {
            u8 amm{}, ass{}, asect{};
            u8 session{};
        } waiting{};
        struct {
            struct {
                u8 amm{}, ass{}, asect{};
            } disc{};
            struct {
                u8 amm{}, ass{}, asect{};
            } track{};
        } last_read{};

        bool needs_seek{};
    } seek;

    i64 seek_cycles();

    struct {
        CDHEAD_MODE mode{};
        // can be seeking,
        // can be reading
        // can be data or audio mode

        u32 sector{}, session{};
        u32 sample_index{}; // 0/2352
        bool muted{false};
        u8 amm{}, ass{}, asect{};
    } head{};

    struct {
        u8 subcmd{};
        u32 counterlo{}, counterhi{};
    } test{};

    CDROM_DISC data{};
    struct {
        u32 num_sessions{false};
        bool inserted{false};
    } disk{};

    struct {
        struct {
            u8 file{}, channel{};
        } filter{};
        XA::DECODER decoder{};

        struct {
            u32 head{}, tail{}, len{};
            u8 bufs[8][0x930];
        } sector_buf;
    } xa{};
    i64 speed_changed{};


private:
    void next_sector();
    u32 read_01(u8 sz, bool has_effect);
    u32 read_02(u8 sz, bool has_effect);
    u32 read_03(u8 sz, bool has_effect);
    void write_01(u32 val, u8 sz);
    void write_02(u32 val, u8 sz);
    void write_03(u32 val, u8 sz);
    void write_CMD(u32 val);
    void reset(); // TODO: CALL THIS
    void recalc_HSTS();
    void schedule_CMD();
    void flush_deferred_IRQs();
    bool deliver_deferred(IRQSTRUCT::DEFERREDSTRUCT::DEFERREDSTRUCTITEM &item);
    void schedule_finish(u64 clock);
    void schedule_read(u64 clock);
    void schedule_step_3(u64 clock);
    void schedule_seek_finish(u64 clock);;
    void cancel_CMD();
    void reset_decoder();
    void finish_CMD(bool do_stat_irq, u32 irq_num);
    void result(u32 lvl, std::initializer_list<u8> rdata);
    void result_error(u8 code);
    void stat_irq();
    void cmd_setfilter();
    void cmd_setloc();
    void cmd_test();
    void cmd_play();
    void cmd_forward();
    void cmd_backward();
    void cmd_reads(u64 clock);
    void cmd_motor_on(u64 clock);
    void cmd_motor_on_finish();
    void cmd_stop(u64 clock);
    void cmd_pause(u64 clock);
    void cmd_mute();
    void cmd_init(u64 clock);
    void cmd_setmode();
    void cmd_reset();
    void cmd_seekl(u64 clock);
    void cmd_getlocp();
    u32 get_LBA_from_aaa(u32 amm, u32 ass, u32 asect);
    u32 get_track_from_LBA(u32 LBA);
    u32 get_cur_LBA() { return get_LBA_from_aaa(seek.last_read.disc.amm, seek.last_read.disc.ass, seek.last_read.disc.asect);}
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
    void update_track_loc();

    void queue_xa_sector(u8 *ptr);
    void get_CD_audio_cdda(i16 &left, i16 &right);
    void get_CD_audio_xa(i16 &left, i16 &right);

    bool xa_decode_next_sector();
    u8 *xa_get_sector(u8 &CI);
    void xa_decode_28(u8 *ptr, u32 blk, u32 nibble, u8 hd, i16 &old, i16 &older, i16 *s_out);
    static i16 zigzaginterp(const i16 *ringbuf, u32 p, const i32 *tabl);
};
}