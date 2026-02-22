//
// Created by . on 3/6/25.
//

#include "helpers/debug.h"
#include "ps1_cdrom.h"
#include "../ps1_debugger.h"
#include "helpers/debugger/debugger.h"
#include "../ps1_bus.h"

namespace PS1::CDROM {

static u32 constexpr masksz[5] = {0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF };
#define CYCLES_PER_SEC 33868800
#define ONEFRAME 564480

#define UKN_TIME ONEFRAME
#define TIME_IN_SEC static_cast<double>(bus->clock.master_cycle_count) / 33868800.0
void FIFO_IRQ::reset() {
    head = tail = len = 0;
    in_results.reset();
    out_results.reset();
}

void FIFO_IRQ::push_irq(u32 val) {
    if (len >= 16) {
        printf("\n(CDR IRQ FIFO)ATTEMPT TO PUSH WHEN FULL!");
        return;
    }
    auto &e = entries[tail];
    e.results.copy(in_results);
    in_results.reset();
    e.val = val;
    tail = (tail + 1) & 15;
    len++;
}

u8 FIFO_IRQ::pop_irq() {
    if (len == 0) { printf("\n(CDR IRQ FIFO)EMPTY POP!"); return 0; }
    auto &e = entries[head];
    head = (head + 1) & 15;
    len--;

    out_results.copy(e.results);
    return e.val;
};

void FIFO_IRQ::push_result(u8 val) {
    in_results.push(val);
}

u8 FIFO_IRQ::pop_result() {
    return out_results.pop();
}

void FIFO::reset() {
    head = tail = len = 0;
    memset(entries, 0, sizeof(entries));
}

void FIFO::push(u32 val) {
    if (len == 16) {
        printf("\n(CDROM) ATTEMPT TO PUSH TO FULL FIFO!");
        dbg_break("CDROM FIFO FULL", 0);
        return;
    }
    entries[tail] = val;
    tail = (tail + 1) & 15;
    len++;
}

u8 FIFO::pop() {
    u8 v = entries[head];
    head = (head + 1) & 15;
    if (len) len--;
    return v;
}

u32 core::mainbus_read(u32 addr, u8 sz, bool has_effect) {
    //printf("\nRD CDROM %08x", addr);
    /*static int a = 0;
    a++;
    if (a == 10) {
        dbg_break("CDROM TEST", 0);
    }*/
    switch (addr) {
        case 0x1F801800: {
            recalc_HSTS();
            u32 v = io.HSTS.u | (io.HSTS.u << 8) | (io.HSTS.u << 16) | (io.HSTS.u << 24);
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "READ HSTS %02x   buf_len:%d  pos/len:%d/%d   num-result:%d", v & 0xFF, sector_buf.len, io.RDDATA.pos, io.RDDATA.len, io.interrupts.out_results.len);
            return v & masksz[sz];
        }
        case 0x1F801801: return read_01(sz, has_effect);
        case 0x1F801802: return read_02(sz, has_effect);
        case 0x1F801803: return read_03(sz, has_effect);
        default:
    }
    NOGOHERE;
}

void core::mainbus_write(u32 addr, u32 val, u8 sz) {
    //printf("\nWR CDROM %08x: %08x", addr, val);
    switch (addr) {
        case 0x1F801800: {
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "WRITE RA %d", val & 3);
            io.HSTS.RA = val & 3;
            return;
        }
        case 0x1F801801: return write_01(val, sz);
        case 0x1F801802: return write_02(val, sz);
        case 0x1F801803: return write_03(val, sz);
        default:
    }
    NOGOHERE;

}

u32 core::read_01(u8 sz, bool has_effect) {
    // RESULT
    u8 v = io.interrupts.pop_result();
    recalc_HSTS();
    dbgloglog_bus(PS1D_CDROM_RESULT, DBGLS_INFO, "POP RESULT %02x", v);
    return v;
}

u32 core::read_02(u8 sz, bool has_effect) {
    // RDDATA
    // if DRQSTS is set, the datablock (disk sector) can be then read from this register
    if (io.HCHPCTL.BFRD && io.HSTS.DRQSTS) {
        u32 v = io.RDDATA.read_byte();
        if (sz >= 2) v |= io.RDDATA.read_byte() << 8;
        if (sz == 4) {
            v |= io.RDDATA.read_byte() << 16;
            v |= io.RDDATA.read_byte() << 24;
        }
        if (io.RDDATA.pos >= io.RDDATA.len) {
            dbgloglog_busn(PS1D_CDROM_RDDATA_FINISH, DBGLS_INFO, "RDDATA consumed");
            io.HSTS.DRQSTS = 0;
        }
        return v;
    }
    return masksz[sz];
}

u32 core::read_03(u8 sz, bool has_effect) {
    u32 v;
    switch (io.HSTS.RA) {
        case 0:
        case 2: // HINTMSK
            v = io.HINTMSK.u | 0b11100000;
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "READ HINTMSK %02x", v);
            return v;
        case 1:
        case 3: // HINTSTS
            v = io.HINTSTS.u | 0b11100000;
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "READ HINTSTS %02x", v);
            return v;
    }
    NOGOHERE;
}

void core::recalc_HSTS() {
    io.HSTS.PRMEMPT = io.PARAMETER.len == 0;
    io.HSTS.PRMWRDY = io.PARAMETER.len != 16;
    io.HSTS.RSLRRDY = io.interrupts.out_results.len > 0;
    io.HSTS.DRQSTS = (io.RDDATA.pos < io.RDDATA.len) && io.HCHPCTL.BFRD;
}

void core::write_CMD(u32 val) {
    // TODO: this
    if (io.HSTS.BUSYSTS) {
        printf("\n(CDROM) WARN IGNORE CMD %02x", val);
        //cancel_CMD();
        return;
    }
    //printf("\n(CDROM) CMD WRITE %02x", val);
    io.CMD = val & 0xFF;
    io.HSTS.BUSYSTS = 1;
    schedule_CMD();
}

void core::result_string(const char *s) {
    for (u32 i = 0; i < strlen(s); i++) {
        if (s[i] != 0) result(s[i]);
    }
}

void core::result(u32 val) {
    dbgloglog_bus(PS1D_CDROM_RESULT, DBGLS_INFO, "PUSH RESULT %02x", val);
    io.interrupts.push_result(val);
}

void core::cmd_start(u64 key, u64 clock) {
    u32 cmd = io.CMD;
    if ((cmd >= 0x20) && (cmd <= 0x4F)) cmd = 0;
    if (cmd >= 0x60) cmd = 0;
    printif(ps1.cdrom.cmd, "\n(CDROM) EXEC CMD %02x", io.CMD);
    dbgloglog_bus(PS1D_CDROM_CMD, DBGLS_INFO, "Exec cmd %02x", io.CMD);
    switch(cmd) {
        case 0x0D: // setfilter
            cmd_setfilter();
            return;
        case 0x00:
        case 0x17:
        case 0x18:
            result(0x11);
            result(0x40);
            queue_interrupt(5);
            finish_CMD(false, 3);
            return;
        case 0x19:
            cmd_test();
            return;
        case 0x58:
        case 0x59:
        case 0x5A:
        case 0x5B:
        case 0x5C:
        case 0x5D:
        case 0x5E:
        case 0x5F:
            finish_CMD(false, 3);
            return;
        case 0x01: // NOP
            stat_irq();
            io.stat.shell_open = 0;
            finish_CMD(false, 3);
            return;
        case 0x02: // SetLoc
            cmd_setloc();
            return;
        case 0x03: // Play
            cmd_play();
            return;
        case 0x04: // Forward
            cmd_forward();
            return;
        case 0x05: // Backward
            stat_irq();
            cmd_backward();
            return;
        case 0x13: // GetTN
            cmd_gettn();
            return;
        case 0x14:
            cmd_gettd();
            return;
        case 0x1B: // ReadS
            dbgloglog_busn(PS1D_CDROM_READ, DBGLS_INFO, "(Cmd) ReadS");
            cmd_reads(clock);
            return;
        case 0x06: // ReadN
            dbgloglog_busn(PS1D_CDROM_READ, DBGLS_INFO, "(Cmd) ReadN");
            cmd_reads(clock);
            return;
        case 0x07:
            cmd_motor_on(clock);
            return;
        case 0x08:
            cmd_stop(clock);
            return;
        case 0x09:
            cmd_pause(clock);
            return;
        case 0x12:
            cmd_set_session(clock);
            return;
        case 0x0a:
            cmd_init(clock);
            return;
        case 0x0C:
            cmd_demute();
            return;
        case 0x0e:
            cmd_setmode();
            return;
        case 0x15: // SeekL
            cmd_seekl(clock);
            return;
        case 0x11:
            cmd_getlocp();
            return;
        case 0x16:
            cmd_seekp(clock);
            return;
        case 0x1A:
            cmd_getid(clock);
            return;
        case 0x1C: // RESET
            cmd_reset();
            return;
    }
    printf("\n\nUnhandled CDROM CMD %02x\n", io.CMD);
}

void core::cmd_step3(u64 key, u64 clock) {
    switch (io.CMD) {
        case 0x12:
            result(0x06);
            result(0x40);
            queue_interrupt(5);
            finish_CMD(false, 0);
            return;
        default:
            NOGOHERE;
    }
}

void core::cmd_finish(u64 key, u64 clock) {
    switch (io.CMD) {
        case 0x07:
            cmd_motor_on_finish();
            return;
        case 0x08: // stop
            io.stat.motor_fullspeed = 0;
            finish_CMD(true, 2);
            return;
        case 0x09:
            io.stat.read = 0;
            io.stat.play = 0;
            finish_CMD(true, 2);
            return;
        case 0x0A: // Init
            finish_CMD(true, 2);
            return;
        case 0x12: // SetSession
            cmd_set_session_finish(clock);
            return;
        case 0x15: // SeekL
        case 0x16: // SeekP
            io.stat.seek = 0;
            seek.needs_seek = false;
            seek.last_read.disc.amm = seek.amm;
            seek.last_read.disc.asect = seek.asect;
            seek.last_read.disc.ass = seek.ass;
            finish_CMD(true, 2);
            return;
        case 0x1A: // GetID
            cmd_getid_finish();
            return;
        default:
            NOGOHERE;
    }
}

static u8 encode_bcd(u8 val) {
    return ((val / 10) << 4) | (val % 10);
}

    static u8 decode_bcd(u8 val) {
    return ((val >> 4) * 10) + (val & 0xF);;
}

void core::cmd_test() {
    u8 sub = io.PARAMETER.pop();
    test.subcmd = sub;
    //printf("\n(CDROM) TEST %02x", sub);
    switch (sub) {
        case 0x20: // CDROM BIOS version 94h,09h,19h,C0h
            result(0x94);
            result(0x09);
            result(0x19);
            result(0xC0);
            queue_interrupt(3);
            finish_CMD(false, 0);
            return;
        case 0x21: { // Status of POS0 and DOOR switches
            u32 v = head.sector == 0;
            v |= io.stat.shell_open << 1;
            result(v);
            queue_interrupt(3);
            finish_CMD(false, 0);
            return; }
        case 0x22: // Not supported/higher BIOS version
            result(0x11);
            result(0x10);
            queue_interrupt(5);
            finish_CMD(false, 0);
            return;
        case 0x23: // Servo Amplifier
        case 0x24: // Signal processor
            result_string("CXD2940Q");
            queue_interrupt(3);
            finish_CMD(false, 0);
            return;
        case 0x25: // Not supported/higher BIOS version
            result(0x11);
            result(0x10);
            queue_interrupt(5);
            return;
        case 0x04: // Read SCEx string (and force motor on)
            if (io.stat.shell_open) {
                test.counterlo = test.counterhi = 0;
            }
            else {
                test.counterlo = test.counterhi = 1;
            }
            finish_CMD(true, 3);
            io.stat.motor_fullspeed = 1;
            return;
        case 0x05: // Read total/success
            result(test.counterlo);
            result(test.counterhi);
            queue_interrupt(3);
            finish_CMD(false, 0);
            return;
    }
    printf("\n(CDROM) TEST UNKNOWN CMD %02x", sub);
}

void core::cmd_setfilter() {
    xa.filter.file = io.PARAMETER.pop();
    xa.filter.channel = io.PARAMETER.pop() & 0x1F; // TODO: ?
    finish_CMD(true, 3);
}

void core::cmd_setloc() {
    //printf("\n(CDROM) CMD SetLoc");
    stat_irq();
    if (!io.MODE.ignore_bit) {
        seek.amm = decode_bcd(io.PARAMETER.pop());
        seek.ass = decode_bcd(io.PARAMETER.pop());
        seek.asect = decode_bcd(io.PARAMETER.pop());
        seek.needs_seek = true;
        dbgloglog_bus(PS1D_CDROM_SETLOC, DBGLS_INFO, "(Cmd) SetLoc %02d:%02d:%02d", seek.amm, seek.ass, seek.asect);
    }
    else {
        dbgloglog_busn(PS1D_CDROM_SETLOC, DBGLS_INFO, "(Cmd) SetLoc ignore_bit set");
    }
    finish_CMD(false, 0);
}

u32 core::get_LBA_from_aaa(u32 amm, u32 ass, u32 asect) {
    return asect + (ass * 75) + (amm * (75 * 60));
}

u32 core::get_track_from_LBA(u32 LBA) {
    // Find current track...
    i32 cur_track = 0;
    for (u32 i = 1; i < (data.num_tracks + 1); i++) {
        auto &t = data.tracks[i-1];
        if (LBA >= t.data_lba) {
            cur_track = i;
            break;
        }
    }
    if (cur_track == 0) {
        printf("\nWARNING COULDNT FIND TRACK");
        cur_track = 1;
    }
    return cur_track;
}

void core::cmd_play() {
    printf("\n\n\nPLAY!");
    dbgloglog_busn(PS1D_CDROM_PLAY, DBGLS_INFO, "(Cmd) Play");
    if (io.stat.seek || io.stat.read || !io.stat.motor_fullspeed) {
        dbgloglog_bus(PS1D_CDROM_PLAY, DBGLS_INFO, "(Cmd) Play error can't. SEEK:%d READ:%d FULLSPEED:%d", io.stat.seek, io.stat.read, io.stat.motor_fullspeed);
        result(0x10);
        result(0x80);
        queue_interrupt(5);
        finish_CMD(false, 0);
    }
    if (io.MODE.report && head.mode == HM_AUDIO) {
        dbgloglog_busn(PS1D_CDROM_PLAY, DBGLS_INFO, "(Cmd) Play req. with reports requested!");
        printf("\nWARN Play with report interrupts done!");
    }
    if (io.PARAMETER.len > 0) {
        u32 track = io.PARAMETER.pop();
        u32 LBA;
        if (track >= data.num_tracks + 1) {
            dbgloglog_busn(PS1D_CDROM_PLAY, DBGLS_INFO, "(Cmd) Play restart current track");

            // Restart current track
            LBA = get_cur_LBA();
            track = get_track_from_LBA(LBA);
        }
        // Seek to track
        if (track > 0) {
            dbgloglog_bus(PS1D_CDROM_PLAY, DBGLS_INFO, "(Cmd) Play seek track start %d", track);
            LBA = data.tracks[track-1].data_lba;
            seek.asect = LBA % 75;
            seek.ass = (LBA / 75) % 60;
            seek.amm = (LBA / (75 * 60));
        }
    }
    io.stat.play = 1;

    finish_CMD(true, 3);
}

void core::cmd_forward() {
    printf("\n(CDROM) CMD Forward. NOT IMPL!");
}

void core::cmd_backward() {
    printf("\n(CDROM) CMD Backward. NOT IMPL!");
}

void core::cmd_reads(u64 clock) {
    //printf("\n(CDROM) CMD ReadS");    printf("\nSCHEDULING END FOR %02x", io.CMD);

    // Read data!
    do_cmd_read(clock);
}

void core::do_cmd_read(u64 clock) {
    if (read.still_sched) {
        printf("\nABORT read req. processing for already going!");
        dbgloglog_busn(PS1D_CDROM_READ, DBGLS_INFO, "(Cmd) ReadN/S ABORT for already going!");
        stat_irq();
        return;
    }
    finish_CMD(true, 3);
    io.stat.play = 0;
    io.stat.seek = seek.needs_seek;
    io.stat.read = !io.stat.seek;
    if (io.stat.seek) clock += seek_cycles();
    schedule_read(clock);
}

void core::cmd_getid(u64 clock) {
    //printf("\n(CDROM) CMD GetID");
    if (io.stat.shell_open) {
        result(0x11);
        result(0x80);
        queue_interrupt(5);
        finish_CMD(false, 0);
        return;
    }
    if (motor.spinning_up) {
        result(0x01);
        result(0x80);
        queue_interrupt(5);
        finish_CMD(false, 0);
        return;
    }

    stat_irq();
    schedule_finish(clock + 0x4AA6);
}

void core::cmd_getid_finish() {
    if (!disk.inserted) {
        result(0x08);
        result(0x40);
        result(0x00);
        result(0x00);
        result(0x00);
        result(0x00);
        result(0x00);
        result(0x00);
        queue_interrupt(3);
        finish_CMD(false, 0);
        return;
    }
    result(0x02);
    result(0x00);
    result(0x20);
    result(0x00);
    result_string("SCEA");
    queue_interrupt(2);
    finish_CMD(false, 0);
}

void core::cmd_seekp(u64 clock) {
    printf("\n(CDROM) CMD SeekP");
    stat_irq();
    io.stat.seek = 1;
    io.stat.read = 0;
    io.stat.play = 0;
    head.mode = HM_AUDIO;
    printf("\nHEAD MODE AUDIO!!!");
    schedule_seek_finish(clock+seek_cycles());
}

void core::cmd_getlocp() {
    u32 track = get_track_from_LBA(get_cur_LBA());
    result(encode_bcd(track)); // track
    result(1); // index
    // Within track
    update_track_loc();
    result(encode_bcd(seek.last_read.track.amm));
    result(encode_bcd(seek.last_read.track.ass));
    result(encode_bcd(seek.last_read.track.asect));

    // Within disk
    result(encode_bcd(seek.last_read.disc.amm));
    result(encode_bcd(seek.last_read.disc.ass));
    result(encode_bcd(seek.last_read.disc.asect));
    queue_interrupt(3);
    finish_CMD(false, 0);
}
void core::cmd_seekl(u64 clock) {
    //printf("\n(CDROM) CMD SeekL");
    dbgloglog_busn(PS1D_CDROM_SEEK, DBGLS_INFO, "(Cmd) Seek");
    stat_irq();
    io.stat.seek = 1;
    io.stat.read = 0;
    io.stat.play = 0;
    head.mode = HM_DATA;
    schedule_seek_finish(clock+seek_cycles());
}

void core::cmd_motor_on(u64 clock) {
    printf("\n(CDROM) CMD MotorOn");
    stat_irq();
    motor.spinning_up = 1;
    schedule_finish(clock + (ONEFRAME * 60));
}

void core::update_track_loc() {
    i32 disc_LBA = get_cur_LBA(); // like 100
    u32 tnum = get_track_from_LBA(disc_LBA);
    tnum--;
    i32 track_LBA = data.tracks[tnum].data_lba; // like 80
    i32 rel_LBA = disc_LBA - track_LBA;
    if (rel_LBA < 0) {
        printf("\nERROR RELATIVE LBA <0");
        return;
    }
    seek.last_read.track.asect = rel_LBA % 75;
    seek.last_read.track.ass = (rel_LBA / 75) % 60;
    seek.last_read.track.amm = (rel_LBA / (75 * 60));
}
#define XA_SUBHEADER_START 16

void core::queue_xa_sector(u8 *ptr) {
    // Check filter to see if we should just discard
    if (io.MODE.xa_filter) {
        u8 filenum = ptr[XA_SUBHEADER_START+0];
        if (filenum != xa.filter.file) {
            printf("\nIGNORE XAPDCM FILE %02x not %02x!", filenum, xa.filter.file);
            return;
        }
        u8 chnum = ptr[XA_SUBHEADER_START+1];
        if (chnum != xa.filter.channel) {
            //printf("\nIGNORE XADPCM CH %02x NOT %02x!", chnum, xa.filter.channel);
            return;
        }
    }
    if (xa.sector_buf.len > 2) {
        printf("\nWARN XA AUDIO UP TO 2 BUFFERS!");
    }
    if (xa.sector_buf.len >= 8) {
        printf("\nWARN DROP XA BUFFER!");
    }
    memcpy(xa.sector_buf.bufs[xa.sector_buf.tail], ptr, 0x930);
    xa.sector_buf.tail = (xa.sector_buf.tail + 1) & 7;
    xa.sector_buf.len++;
}

void core::read_sector() {
    //printf("\n(CDROM) Read sector %02d:%02d:%02d", seek.amm, seek.ass, seek.asect);
    dbgloglog_bus(PS1D_CDROM_SECTOR_READS, DBGLS_INFO, "(RDSEC) Read sector %02d:%02d:%02d into queue size %d", seek.amm, seek.ass, seek.asect, sector_buf.len);
    // 25:14:48
    seek.last_read.disc.amm = seek.amm;
    seek.last_read.disc.asect = seek.asect;
    seek.last_read.disc.ass = seek.ass;

    if (seek.needs_seek || io.stat.seek) {
        seek.needs_seek = false;
        io.stat.seek = 0;
        io.stat.read = 1;
    }
    /*if ((seek.amm == 25) && (seek.ass == 14) && (seek.asect == 48)) {
        //dbg_break("TARGET READ", 0);
    }*/
    u8 *ptr = data.ptr_to_data(seek.amm, seek.ass, seek.asect);
    if (io.MODE.xa_adpcm) {
        u8 v = ptr[XA_SUBHEADER_START+2];
        if (v & 0x40) { // real-time
            if ((v & 0x4)) {// || // audio
                //(v & 0x8)) { // data, real-time data = audio
                //if (v & 0x8) printf("\nWARN REALTIME DATA!");
                queue_xa_sector(ptr);
                return;
            }
        }
    }

    memcpy(sector_buf.bufs[sector_buf.tail], ptr, 0x930);
    sector_buf.tail = (sector_buf.tail + 1) & 7;
    sector_buf.len++;
    if (sector_buf.len > 1) {
        printf("\nWARN SECTOR BUF LEN %d", sector_buf.len);
        dbg_break("SECTOR BUF FILL", 0);
    }
    result(io.stat.u);
    queue_interrupt(1);
}

void core::sch_read(u64 key, u64 clock) {
    read_sector();
    next_sector();
    schedule_read(clock);
}

void core::next_sector() {
    seek.asect++;
    if (seek.asect >= 75) {
        seek.asect = 0;
        seek.ass++;
        if (seek.ass>=60) {
            seek.ass = 0;
            seek.amm++;
            if (seek.amm >= 74) {
                seek.amm = 0;
            }
        }
    }
}

void core::get_CD_audio(i16 &left, i16 &right) {
    left = right = 0;
    if (io.MODE.xa_adpcm) {
        get_CD_audio_xa(left, right);
    }
    else {
        if (!io.stat.play) return;
        if (head.mode != HM_AUDIO) return;
        get_CD_audio_cdda(left, right);
    }
}

u8 *core::xa_get_sector(u8 &CI) {
    if (xa.sector_buf.len == 0) {
        return nullptr;
    }
    CI = xa.sector_buf.bufs[xa.sector_buf.head][XA_SUBHEADER_START+3];
    u8 *r = xa.sector_buf.bufs[xa.sector_buf.head];
    xa.sector_buf.head = (xa.sector_buf.head + 1) & 7;
    xa.sector_buf.len--;
    return r + 0x18;
}

void core::xa_decode_28(u8 *ptr, u32 blk, u32 nibble, u8 hd, i16 &old, i16 &older, i16 *s_out) {
    /*
    shift  = 12 - (src[4+blk*2+nibble] AND 0Fh)
    filter =      (src[4+blk*2+nibble] AND 30h) SHR 4
    f0 = pos_xa_adpcm_table[filter]
    f1 = neg_xa_adpcm_table[filter]
    for j=0 to 27
    t = signed4bit((src[16+blk+j*4] SHR (nibble*4)) AND 0Fh)
    s = (t SHL shift) + ((old*f0 + older*f1+32)/64);
    s = MinMax(s,-8000h,+7FFFh)
    halfword[dst]=s, dst=dst+2, older=old, old=s
    next j
    */
    u8 shift = hd & 0xF;
    if (shift > 13) shift = 9;
    shift = 12 - shift;
    u8 filter = (hd >> 4) & 7;
    if (filter > 3) filter = 3; // TODO: ??
    u32 sn = xa.decoder.samples.pos;
    for (u32 i = 0; i < 28; i++) {
        i32 t;
        if (nibble == 0) t = ptr[i] & 0x0F;
        else t = ptr[i] >> 4;
        t = (t & 7) | (t & 8 ? 0xFFFFFFF8 : 0);
        i32 s = (t << shift);
        i32 filtered;
        switch (filter) {
            case 0:
                filtered = s;
                break;
            case 1:
                filtered = s + (60 * old + 32) / 64;
                break;
            case 2:
                filtered = s + (115 * old - 52 * older + 32) / 64;
                break;
            case 3:
                filtered = s + (98 * old - 55 * older + 32) / 64;
                break;
            default:
                NOGOHERE;
        }
        if (filtered < -0x8000) s = -0x8000;
        if (filtered > 0x7FFF) s = 0x7FFF;
        *s_out = static_cast<i16>(filtered);
        s_out++;
        older = old;
        old = static_cast<i16>(filtered);
        sn++;
    }
}

static constexpr i16 zzt[7][29] = {
{ // Table[0]
    0, 0, 0, 0, 0,
    -0x0002, 0x000A, -0x0022,
    0x0041, -0x0054, 0x0034, 0x0009, 
    0x010A, 0x0400, -0x0A78, 0x234C,
    0x6794, -0x1780, 0x0BCD, -0x0623,
    0x0350, -0x016D, 0x006B, 0x000A,
    -0x0010, 0x0011, -0x0008, 0x0003,
    -0x0001 },
{ // Table[1]
        0, 0, 0, -0x0002,
        0, 0x0003, -0x0013, 0x003C,
        -0x004B, 0x00A2, -0x00E3, 0x0132,
        -0x0043, -0x0267, 0x0C9D, 0x74BB,
        -0x11B4, 0x09B8, -0x05BF, 0x0372,
        -0x01A8, 0x00A6, -0x001B, 0x0005,
        0x0006, -0x0008, 0x0003, -0x0001,
        0 },
{ // Table[2]
        0, 0, -0x0001, 0x0003,
        -0x0002, -0x0005, 0x001F, -0x004A,
        0x00B3, -0x0192, 0x02B1, -0x039E,
        0x04F8, -0x05A6, 0x7939, -0x05A6,
        0x04F8, -0x039E, 0x02B1, -0x0192,
        0x00B3, -0x004A, 0x001F, -0x0005,
        -0x0002, 0x0003, -0x0001, 0,
        0 },
{ // Table[3]
        0, -0x0001, 0x0003, -0x0008,
        0x0006, 0x0005, -0x001B, 0x00A6,
        -0x01A8, 0x0372, -0x05BF, 0x09B8,
        -0x11B4, 0x74BB, 0x0C9D, -0x0267,
        -0x0043, 0x0132, -0x00E3, 0x00A2,
        -0x004B, 0x003C, -0x0013, 0x0003,
        0     , -0x0002, 0     , 0     ,
    0 },
    { // Table[4]
        -0x0001, 0x0003, -0x0008, 0x0011,
        -0x0010, 0x000A, 0x006B, -0x016D,
        0x0350, -0x0623, 0x0BCD, -0x1780,
        0x6794, 0x234C, -0x0A78, 0x0400,
        -0x010A, 0x0009, 0x0034, -0x0054,
        0x0041, -0x0022, 0x000A, -0x0001,
        0     , 0x0001, 0     , 0     ,
        0 },
    { // Table[5]
        0x0002, -0x0008, 0x0010, -0x0023,
        0x002B, 0x001A, -0x00EB, 0x027B,
        -0x0548, 0x0AFA, -0x16FA, 0x53E0,
        0x3C07, -0x1249, 0x080E, -0x0347,
        0x015B, -0x0044, -0x0017, 0x0046,
        -0x0023, 0x0011, -0x0005, 0     ,
        0     , 0     , 0     , 0     ,
        0 },
    { // Table[6]
        -0x0005, 0x0011, -0x0023, 0x0046,
        -0x0017, -0x0044, 0x015B, -0x0347,
        0x080E, -0x1249, 0x3C07, 0x53E0,
        -0x16FA, 0x0AFA, -0x0548, 0x027B,
        -0x00EB, 0x001A, 0x002B, -0x0023,
        0x0010, -0x0008, 0x0002, 0,
        0, 0, 0, 0,
        0 }
};
    
bool core::xa_decode_next_sector() {
    printf("\nDECODE XA SECTOR!");
    u8 CI;
    u8 *dat = xa_get_sector(CI);
    if (dat == nullptr) {
        //printf("\nNO DATA...");
        return false;
    }
    auto channels = static_cast<XA::XA_CH>(CI & 3);
    auto sample_rate = static_cast<XA::XA_SR>((CI >> 2) & 3);
    auto bps = static_cast<XA::XA_BPS>((CI >> 4) & 3);
    printf("\nXA SECTOR CH:%d SR:%d BPS:%d", channels == XA::STEREO ? 2 : 1, sample_rate == XA::SR37800 ? 37800 : 18900, bps == XA::BITS4 ? 4 : 8);
    bool emphasis = (CI >> 6) & 1;
    if (channels > 1) {
        printf("\n(XA) ERROR INVALID CHANNELS %d", channels);
        channels = XA::STEREO;
    }
    if (bps == XA::BITS8) {
        printf("\n(XA) ERROR 8BIT!");
        dbg_break("8BIT XA-ADPCM!", 0);
        return false;
    }
    if (bps > 1) {
        printf("\n(XA) ERROR INVALID BPS %d DEAULT TO 4", bps);
        bps = XA::BITS4;
    }
    if (sample_rate > 1) {
        printf("\n(XA) ERROR INVALID SAMPLE RATE %d DEAULT TO 37800", sample_rate);
        sample_rate = XA::SR37800;
    }
    u8 headers[8];
    xa.decoder.samples.pos = 0;
    for (u32 block_num = 0; block_num < 0x12; block_num++) {
        dat += 4;
        memcpy(headers, dat, 8);
        dat += 4;
        u8 hdr = 0;
        for (u32 blk = 0; blk < 4; blk++) {
            if (channels == XA::STEREO) {
                xa_decode_28(dat, blk, headers[hdr++], 0, xa.decoder.l_old, xa.decoder.l_older, &xa.decoder.samples.l[xa.decoder.samples.pos]);
                xa_decode_28(dat, blk, headers[hdr++], 1, xa.decoder.r_old, xa.decoder.r_older, &xa.decoder.samples.r[xa.decoder.samples.pos]);
            }
            else {
                xa_decode_28(dat, blk, headers[hdr++], 0, xa.decoder.l_old, xa.decoder.l_older, &xa.decoder.samples.l[xa.decoder.samples.pos]);
                xa.decoder.samples.pos += 28;
                xa_decode_28(dat, blk, headers[hdr++], 1, xa.decoder.l_old, xa.decoder.l_older, &xa.decoder.samples.l[xa.decoder.samples.pos]);
            }
            xa.decoder.samples.pos += 28;
            dat += 28;
        }
    }
    xa.decoder.samples.len = xa.decoder.samples.pos;
    xa.decoder.samples.pos = 0;
    printf("\nDECODED %d XA S", xa.decoder.samples.len);
    // Now zigzag it!
    u32 s_index=0;
    u32 out_index=0;
    u32 num_loops = xa.decoder.samples.len / 6;
    for (u32 i = 0; i < num_loops; i++) {
        // First we need to output 6 samples
        for (u32 u = 0; u < 3; u++) { // 3 * 2 = 6 samples @ 38700
            if (sample_rate == XA::SR37800) {
                // push 2 samples
                xa.decoder.ringbuf.l[xa.decoder.ringbuf.pos] = xa.decoder.samples.l[s_index];
                xa.decoder.ringbuf.l[xa.decoder.ringbuf.pos+1] = xa.decoder.samples.l[s_index+1];
                if (channels == XA::STEREO) {
                    xa.decoder.ringbuf.r[xa.decoder.ringbuf.pos] = xa.decoder.samples.r[s_index];
                    xa.decoder.ringbuf.r[xa.decoder.ringbuf.pos+1] = xa.decoder.samples.r[s_index+1];
                }
                s_index += 2;
            }
            else {
                xa.decoder.ringbuf.l[xa.decoder.ringbuf.pos] = xa.decoder.samples.l[s_index];
                xa.decoder.ringbuf.l[xa.decoder.ringbuf.pos+1] = 0;
                if (channels == XA::STEREO) {
                    xa.decoder.ringbuf.r[xa.decoder.ringbuf.pos] = xa.decoder.samples.l[s_index];
                    xa.decoder.ringbuf.r[xa.decoder.ringbuf.pos+1] = 0;
                }
                s_index++;
                // push 1 sample then 0
            }
            xa.decoder.ringbuf.pos = (xa.decoder.ringbuf.pos + 2) & 0x1F;
        }
        // Now that we've filled 6 samples (or 6 interp. @ 37800), out 7 44.1kHz!
        for (u32 tblnum = 0; tblnum < 7; tblnum++) {
            xa.decoder.out_samples.l[out_index] = zigzaginterp(xa.decoder.ringbuf.l, xa.decoder.ringbuf.pos, &zzt[tblnum][0]);
            if (channels == XA::STEREO) {
                xa.decoder.out_samples.r[out_index] = zigzaginterp(xa.decoder.ringbuf.r, xa.decoder.ringbuf.pos, &zzt[tblnum][0]);
            }
            else {
                xa.decoder.out_samples.r[out_index] = xa.decoder.out_samples.l[out_index];
            }
            out_index++;
        }
    }
    xa.decoder.out_samples.len = out_index;
    printf("\nDECODED %d XA-ADPCM SAMPLES!", xa.decoder.out_samples.len);
    xa.decoder.out_samples.pos = 0;
    return true;
}

void core::get_CD_audio_xa(i16 &left, i16 &right) {
    if (xa.decoder.out_samples.pos >= xa.decoder.out_samples.len) if (!xa_decode_next_sector()) return;
    // Advance in our current block!
    left = xa.decoder.out_samples.l[xa.decoder.out_samples.pos];
    right = xa.decoder.out_samples.r[xa.decoder.out_samples.pos];
    xa.decoder.out_samples.pos++;
}

i16 core::zigzaginterp(i16 *ringbuf, u32 p, const i16 *tabl) {
    i32 sum = 0;
    //p = (p - 1) & 0x1F;
    for (u32 i = 0; i < 29; i++) {
        sum +=(ringbuf[(p-i) & 0x1F]* tabl[i]) >> 15;
    }
    if (sum < -0x8000) sum = -0x8000;
    if (sum > 0x7FFF) sum = 0x7FFF;
    return static_cast<i16>(sum);
}

void core::get_CD_audio_cdda(i16 &left, i16 &right) {
    //u32 addr = head.sample_index * 4;
    u32 LBA = seek.asect + (seek.ass * 75) + (seek.amm * 75 * 50);

    auto *smp = reinterpret_cast<i16 *>(data.data.ptr + (LBA * 2352));
    smp += head.sample_index * 2;
    head.sample_index++;
    if (head.sample_index >= 588) {
        head.sample_index = 0;
        next_sector();
    }

    i32 l = smp[0];
    i32 r = smp[1];
    // Now mix the samples in...
    i32 l_o = left = 0;
    i32 r_o = right = 0;
    if (head.muted) return;

    // 0x40 = half
    // 0x80 = full
    // 0xFF = double
    // so... multiply then >> 7?
    l_o += (l * io.L_L) >> 7;
    l_o += (r * io.R_L) >> 7;
    r_o += (l * io.L_R) >> 7;
    r_o += (r * io.R_R) >> 7;
    if (l_o < -0x8000) l_o = -0x8000;
    if (l_o > 0x7FFF) l_o = 0x7FFF;
    if (r_o < -0x8000) r_o = -0x8000;
    if (r_o > 0x7FFF) r_o = 0x7FFF;
    left = static_cast<i16>(l_o);
    right = static_cast<i16>(r_o);
}

void core::cmd_motor_on_finish() {
    if (io.stat.motor_fullspeed) {
        // Give error INT5!
        result(io.stat.u);
        result(0x20);
        queue_interrupt(5);
    }
    else {
        io.stat.motor_fullspeed = 1;
        motor.spinning_up = 0;
        result(io.stat.u);
        queue_interrupt(2);
    }
    finish_CMD(false, 3);
}

void core::cmd_stop(u64 clock) {
    printf("\n(CDROM) CMD Stop");
    io.stat.seek = 0;
    stat_irq();
    schedule_finish(clock + (ONEFRAME * 60 * 2));
}

void core::cmd_pause(u64 clock) {
    //printf("\n(CDROM) CMD Pause");
    dbgloglog_busn(PS1D_CDROM_PAUSE, DBGLS_INFO, "(Cmd) Pause");
    stat_irq();
    if (read.still_sched) bus->scheduler.delete_if_exist(read.sched_id);
    io.stat.read = 0;
    schedule_finish(clock + ONEFRAME/60);
}

void core::cmd_gettn() {
    // int3 stat,first,last
    result(io.stat.u);
    result(1);
    result(data.num_tracks);
    queue_interrupt(3);
    printf("\nGetTN NUMBER OF TRACKS: %d", data.num_tracks);
    finish_CMD(false, 0);
}

void core::cmd_gettd() {
    u32 track_num = decode_bcd(io.PARAMETER.pop());
    printf("\n(CDROM) CMD GetTD %d", track_num);
    if (track_num > data.num_tracks) {
        result(0x10);
        queue_interrupt(5);
        finish_CMD(false, 0);
        return;
    }
    result(io.stat.u);
    // mm, ss
    u32 calc;
    if (track_num == 0) {
        // End of last track
        calc = data.end_of_last_track;
    }
    else {
        calc = data.tracks[track_num-1].data_lba;
        if (track_num != 1) calc += 150;
    }
    // ((mm * 60) + ss) * 75) + sect
    static constexpr u32 min_val = 60 * 75;
    static constexpr u32 sec_val = 75;
    u32 mm = calc / min_val;
    u32 ss = (calc % min_val) / sec_val;
    result(encode_bcd(mm));
    result(encode_bcd(ss));
    queue_interrupt(3);
    finish_CMD(false, 0);
}

void core::cmd_demute() {
    head.muted = false;
    finish_CMD(true, 3);

}

void core::cmd_init(u64 clock) {
    //printf("\n(CDROM) CMD Init");
    stat_irq();
    io.MODE.u = 0x20;
    io.stat.motor_fullspeed = 1;
    schedule_finish(clock + UKN_TIME);
}

void core::cmd_setmode() {
    u8 mode = io.PARAMETER.pop();
    //printf("\n(CDROM) CMD SETMODE %02x", mode);
    dbgloglog_bus(PS1D_CDROM_READ, DBGLS_INFO, "(Cmd) SetMode %02x", mode);
    if (!(mode & 0x10)) {
        io.latch.MODE_sector_size = (mode >> 5) & 1;
        //io.latch.MODE_sector_size = 0;
    }
    io.MODE.u = mode;
    finish_CMD(true, 3);
}

void core::cmd_reset() {
    printf("\n(CDROM) CMD RESET.");
    finish_CMD(true, 3);
    sector_buf.head = sector_buf.tail = sector_buf.len = 0;
    io.interrupts.reset();
    io.PARAMETER.reset();
}

void core::cmd_set_session(u64 clock) {
    //printf("\n(CDROM) CMD SetSession");
    seek.session = io.PARAMETER.pop();
    if (seek.session == 0) {
        result(0x03);
        result(0x10);
        queue_interrupt(5);
        finish_CMD(false, 0);
        return;
    }
    if (io.stat.play || io.stat.read) {
        result(0x80);
        queue_interrupt(5);
        printf("\nWARN SETSESSION ERR!");
        return;
    }
    stat_irq();
    io.stat.seek = 1;
    io.stat.read = 0;
    io.stat.play = 0;
    schedule_finish(clock + UKN_TIME);
}

void core::cmd_set_session_finish(u64 clock) {
    io.stat.seek = 0;
    if (disk.num_sessions == 1){
        if (seek.session > 1) {
            result(0x06);
            result(0x40);
            queue_interrupt(5);
            schedule_step_3(clock + UKN_TIME);
            io.stat.seek = 1;
        }
        else {
            head.session = seek.session;
            finish_CMD(true, 2);
        }
    }
    else {
        if (seek.session >= (disk.num_sessions + 1)) {
            result(0x06);
            result(0x20);
            queue_interrupt(5);
            finish_CMD(false, 0);
        }
        else {
            head.session = seek.session;
            finish_CMD(true, 2);
        }
    }
}

//void core::cmd_() {
//    printf("\n(CDROM) CMD . NOT IMPL!");
//}

static void scheduled_cmd_start(void *ptr, u64 key, u64 timecode, u32 jitter) {
    auto *th = static_cast<core *>(ptr);
    th->cmd_start(key, timecode - jitter);
}

static void scheduled_read(void *ptr, u64 key, u64 timecode, u32 jitter) {
    auto *th = static_cast<core *>(ptr);
    th->sch_read(key, timecode - jitter);
}

static void scheduled_cmd_end(void *ptr, u64 key, u64 timecode, u32 jitter) {
    auto *th = static_cast<core *>(ptr);
    th->cmd_finish(key, timecode - jitter);
}

static void scheduled_cmd_step3(void *ptr, u64 key, u64 timecode, u32 jitter) {
    auto *th = static_cast<core *>(ptr);
    th->cmd_step3(key, timecode - jitter);
}

void core::schedule_read(u64 clock) {
    u64 div = io.MODE.speed ? 150 : 75;
    u64 tm = clock + (CYCLES_PER_SEC / div);

    read.sched_id = bus->scheduler.only_add_abs(tm, 0, this, &scheduled_read, &read.still_sched);
}

void core::schedule_finish(u64 clock) {
    //printf("\nSCHEDULING END FOR %02x", io.CMD);
    CMD.sched_id = bus->scheduler.only_add_abs(clock, 0, this, &scheduled_cmd_end, &CMD.still_sched);
}

void core::schedule_step_3(u64 clock) {
    CMD.sched_id = bus->scheduler.only_add_abs(clock, 0, this, &scheduled_cmd_step3, &CMD.still_sched);
}

i64 core::seek_cycles() {
    return (ONEFRAME*24);
}

void core::schedule_seek_finish(u64 clock) {
    CMD.sched_id = bus->scheduler.only_add_abs(clock, 0, this, &scheduled_cmd_end, &CMD.still_sched);
}

void core::schedule_CMD() {

#define SC(y) CMD.sched_id = bus->scheduler.only_add_abs(bus->scheduler.current_time() + y, 0, this, &scheduled_cmd_start, &CMD.still_sched)
    // TODO: this
    switch (io.CMD) {
        case 0x0A: // Init
        case 0x1E: // ReadTOC
            SC(81102);
            return;
        default:
            if (io.stat.motor_fullspeed) SC(50401);
            else SC(23796);
            return;
    }
    NOGOHERE;
#undef STD
}

void core::queue_interrupt(u32 level) {
    //printf("\n(CDROM) QUEUE INT %d", level);
    if (io.HINTSTS.INTSTS == 0) {
        io.HINTSTS.INTSTS = level;
        io.interrupts.out_results.copy(io.interrupts.in_results);
        io.interrupts.in_results.reset();
        dbgloglog_bus(PS1D_CDROM_IRQ_ASSERT, DBGLS_INFO, "IRQ assert: %d  CMD:%02x", level, io.CMD);
        recalc_HSTS();
        update_IRQs();
    }
    else {
        dbgloglog_bus(PS1D_CDROM_IRQ_QUEUE, DBGLS_INFO, "IRQ queued %d num_waiting_before:%d", level, io.interrupts.len);
        io.interrupts.push_irq(level);
    }
}

void core::stat_irq() {
    result(io.stat.u);
    queue_interrupt(3);
}

void core::finish_CMD(bool do_stat_irq, u32 irq_num) {
    io.HSTS.BUSYSTS = 0;
    if (io.interrupts.in_results.len > 0) {
        printf("\nWARN CMD %02x STILL HAS IN RESULTS: %d", io.CMD, io.interrupts.in_results.len);
    }
    //printif(ps1.cdrom.cmd, "\n(CDROM) CMD FINISH");
    //printf("\nfinish_CMD %02x", io.CMD);

    if (do_stat_irq) {
        result(io.stat.u);
        queue_interrupt(irq_num);
    }
    // TODO: parameter reset?     io.PARAMETER.reset();
    dbgloglog_busn(PS1D_CDROM_FINISH_CMD, DBGLS_INFO, "(Cmd) finish");
}

void core::cancel_CMD() {
    if (CMD.still_sched) {
        printf("\nCANCEL %02x", io.CMD);
        bus->scheduler.delete_if_exist(CMD.sched_id);
    }
    else {
        printf("\n(CDROM) CMD NOT SCHEDULED??");
    }
}

void core::write_01(u32 val, u8 sz) {
    switch (io.HSTS.RA) {
        case 0: // CMD
            write_CMD(val);
            return;
        case 1: // WRDATA
            // Used to write sectors for XA-ADPCM so ignore for now!
            printf("\n(CDROM) WRDATA write %02x", val);
            return;
        case 2: // CI
            io.CI.u = val & 0b01010101;
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "WRITE CI %02x", val);
            //printf("\n(CDROM) CI write %02x", io.CI.u);
            return;
        case 3: // ATV2 R->R
            io.R_R = val & 0xFF;;
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "WRITE R->R %02x", val);
            return;
    }
}

void core::remove_disc() {
    disk.inserted = false;
}

void core::open_drive() {
    io.stat.shell_open = 1;
    io.stat.motor_fullspeed = 0;
}

void core::close_drive() {
    io.stat.shell_open = 0;
}

void core::insert_disc(multi_file_set &mfs) {
    printf("\nInserting PSX CDROM! DO THIS!!!");
    io.stat.shell_open = 0;
    data.parse_cue(mfs);
    disk.inserted = true;
}

void core::update_IRQs() {
    u32 lvl = (io.HINTMSK.u & io.HINTSTS.u & 0b11111) != 0;
    set_irq_lvl(set_irq_ptr, lvl);
    //printf("\n(CDROM) IRQ update %d", lvl);
}

void core::write_02(u32 val, u8 sz) {
    switch (io.HSTS.RA) {
        case 0: // PARAMETER
            //printf("\n(CDROM) PARAMETER write %02x", val);
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "WRITE PARAMETER %02x", val);
            io.PARAMETER.push(val);
            recalc_HSTS();
            return;
        case 1: {
            // HINTMSK
            u32 old = io.HINTMSK.u;
            io.HINTMSK.u = (val & 0b11111) | 0b11100000;
            if (io.HINTMSK.u != old) update_IRQs();
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "WRITE HINTMASK %02x", val);
            return; }
        case 2: // ATV0 L->L
            io.L_L = val & 0xFF;
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "WRITE L->L %02x", val);
            return;
        case 3: // ATV3 R->L
            io.R_L = val & 0xFF;
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "WRITE R->L %02x", val);
            return;
    }
}
void core::queue_sector_RDDATA() {
    if (sector_buf.len == 0) {
        dbgloglog_busn(PS1D_CDROM_RDDATA_START, DBGLS_WARN, "WARN: RDDATA requested with no buffers.");
        printf("\nNO BUFS TO RDDATA!?");
        return;
    }
    if (io.RDDATA.pos < io.RDDATA.len) {
        dbgloglog_bus(PS1D_CDROM_RDDATA_START, DBGLS_INFO, "RDDATA when pos/len:%d/%d", io.RDDATA.pos, io.RDDATA.len);
    }
    io.RDDATA.clear();
    io.RDDATA.ptr = sector_buf.bufs[sector_buf.head];
    if (io.latch.MODE_sector_size) {
        io.RDDATA.len = 0x924;
        io.RDDATA.ptr += 0x0C; // drop sync bytes
    }
    else {
        io.RDDATA.len = 0x800;
        io.RDDATA.ptr += 0x18;
    }
    sector_buf.head = (sector_buf.head + 1) & 7;
    sector_buf.len--;
    dbgloglog_bus(PS1D_CDROM_RDDATA_START, DBGLS_INFO, "RDDATA request sz:%03x  sectors left in buffer:%d. num_irqs_queued:%d", io.RDDATA.len, sector_buf.len, io.interrupts.len);
}

void core::write_03(u32 val, u8 sz) {
    switch (io.HSTS.RA) {
        case 0:
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "WRITE HCHPCTL %02x", val);
            io.HCHPCTL.u = val & 0b11100000;
            if (io.HCHPCTL.BFRD && sector_buf.len > 0) {
                queue_sector_RDDATA();
            }
            else if (!io.HCHPCTL.BFRD) {
                //io.RDDATA.pos = 0;
                // TODO: cna break some games!?!?
            }
            recalc_HSTS();
            return;
        case 1: {
            // HCLRCTL
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "WRITE HCLRCTL %02x", val);
            u32 old = io.HINTMSK.u;
            io.HINTSTS.u &= ((val & 0b11111) ^ 0b11111);
            io.HINTSTS.u |= 0b11100000;
            //printf("\n(CDROM) reset RESULTs on HCLRCTL write");
            if (io.HINTSTS.INTSTS == 0) {
                io.interrupts.out_results.reset();
                if (io.interrupts.len > 0) {
                    io.HINTSTS.INTSTS = io.interrupts.pop_irq();
                    dbgloglog_bus(PS1D_CDROM_IRQ_ASSERT, DBGLS_INFO, "IRQ assert: %d num_left:%d", io.HINTSTS.INTSTS, io.interrupts.len);
                }
            }
            if (old != io.HINTSTS.u) update_IRQs();
            //printf("\n(CDROM) HCLRCTL write %02x. new HINTSTS %02x", val, io.HINTSTS.u);
            if (val & 0x80) {
                //printf("\n(CDROM) Reset decoder");
                reset_decoder();
            }
            if (val & 0x40) {
                //printf("\n(CDROM) Clear parameter stack");
                io.PARAMETER.reset();
            }
             if (val & 0x20) {
                 printf("\n(CDROM) Clear sound map XA-ADPCM buffer");
                 // Clear sound map XA-ADPCM buffer...!?!?!
             }
            return;
        }
        case 2: // ATV1 L->R
            io.L_R = val & 0xFF;
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "WRITE L->R %02x", val);
            return;
        case 3: // ADPCTL
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "WRITE ADPCTL %02x", val);

            if (val & 1) {
                // TODO: mute XA-ADPCM!
            }
            if (val & 0x20) {
                io.latch.L_L = io.L_L;
                io.latch.L_R = io.L_R;
                io.latch.R_L = io.R_L;
                io.latch.R_R = io.R_R;
            }
            return;
    }
}
void core::reset_decoder() {
    printf("\n(CDROM)  WARN  reset decoder!");
}

void core::reset() {
    io.HSTS.u = 0b00001000;
}

}
