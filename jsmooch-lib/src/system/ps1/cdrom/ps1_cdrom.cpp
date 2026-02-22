//
// Created by . on 3/6/25.
//

#include "helpers/debug.h"
#include "ps1_cdrom.h"
#include "../ps1_debugger.h"
#include "helpers/debugger/debugger.h"
#include "../ps1_bus.h"

namespace PS1 {

static u32 constexpr masksz[5] = {0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF };
#define ONEFRAME (33868800/60)
#define UKN_TIME ONEFRAME

void CD_FIFO_IRQ::reset() {
    head = tail = len = 0;
    in_results.reset();
    out_results.reset();
}

void CD_FIFO_IRQ::push_irq(u32 val) {
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

u8 CD_FIFO_IRQ::pop_irq() {
    if (len == 0) { printf("\n(CDR IRQ FIFO)EMPTY POP!"); return 0; }
    auto &e = entries[head];
    head = (head + 1) & 15;
    len--;

    out_results.copy(e.results);
    return e.val;
};

void CD_FIFO_IRQ::push_result(u8 val) {
    in_results.push(val);
}

u8 CD_FIFO_IRQ::pop_result() {
    return out_results.pop();
}

void CD_FIFO::reset() {
    head = tail = len = 0;
    memset(entries, 0, sizeof(entries));
}

void CD_FIFO::push(u32 val) {
    if (len == 16) {
        printf("\n(CDROM) ATTEMPT TO PUSH TO FULL FIFO!");
        dbg_break("CDROM FIFO FULL", 0);
        return;
    }
    entries[tail] = val;
    tail = (tail + 1) & 15;
    len++;
}

u8 CD_FIFO::pop() {
    u8 v = entries[head];
    head = (head + 1) & 15;
    if (len) len--;
    return v;
}

u32 CDROM::mainbus_read(u32 addr, u8 sz, bool has_effect) {
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

void CDROM::mainbus_write(u32 addr, u32 val, u8 sz) {
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

u32 CDROM::read_01(u8 sz, bool has_effect) {
    // RESULT
    u8 v = io.interrupts.pop_result();
    recalc_HSTS();
    dbgloglog_bus(PS1D_CDROM_RESULT, DBGLS_INFO, "POP RESULT %02x", v);
    return v;
}

u32 CDROM::read_02(u8 sz, bool has_effect) {
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

u32 CDROM::read_03(u8 sz, bool has_effect) {
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

void CDROM::recalc_HSTS() {
    io.HSTS.PRMEMPT = io.PARAMETER.len == 0;
    io.HSTS.PRMWRDY = io.PARAMETER.len != 16;
    io.HSTS.RSLRRDY = io.interrupts.out_results.len > 0;
    io.HSTS.DRQSTS = (io.RDDATA.pos < io.RDDATA.len) && io.HCHPCTL.BFRD;
}

void CDROM::write_CMD(u32 val) {
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

void CDROM::result_string(const char *s) {
    for (u32 i = 0; i < strlen(s); i++) {
        if (s[i] != 0) result(s[i]);
    }
}

void CDROM::result(u32 val) {
    dbgloglog_bus(PS1D_CDROM_RESULT, DBGLS_INFO, "PUSH RESULT %02x", val);
    io.interrupts.push_result(val);
}

void CDROM::cmd_start(u64 key, u64 clock) {
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

void CDROM::cmd_step3(u64 key, u64 clock) {
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

void CDROM::cmd_finish(u64 key, u64 clock) {
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

void CDROM::cmd_test() {
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

void CDROM::cmd_setfilter() {
    xa.filter.file = io.PARAMETER.pop();
    xa.filter.channel = io.PARAMETER.pop() & 0x1F; // TODO: ?
    finish_CMD(true, 3);
}

void CDROM::cmd_setloc() {
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

u32 CDROM::get_LBA_from_aaa(u32 amm, u32 ass, u32 asect) {
    return asect + (ass * 75) + (amm * (75 * 60));
}

u32 CDROM::get_track_from_LBA(u32 LBA) {
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

void CDROM::cmd_play() {
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

void CDROM::cmd_forward() {
    printf("\n(CDROM) CMD Forward. NOT IMPL!");
}

void CDROM::cmd_backward() {
    printf("\n(CDROM) CMD Backward. NOT IMPL!");
}

void CDROM::cmd_reads(u64 clock) {
    //printf("\n(CDROM) CMD ReadS");    printf("\nSCHEDULING END FOR %02x", io.CMD);

    // Read data!
    do_cmd_read(clock);
}

void CDROM::do_cmd_read(u64 clock) {
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

void CDROM::cmd_getid(u64 clock) {
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

void CDROM::cmd_getid_finish() {
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

void CDROM::cmd_seekp(u64 clock) {
    printf("\n(CDROM) CMD SeekP");
    stat_irq();
    io.stat.seek = 1;
    io.stat.read = 0;
    io.stat.play = 0;
    head.mode = HM_AUDIO;
    printf("\nHEAD MODE AUDIO!!!");
    schedule_seek_finish(clock+seek_cycles());
}

void CDROM::cmd_getlocp() {
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
void CDROM::cmd_seekl(u64 clock) {
    //printf("\n(CDROM) CMD SeekL");
    dbgloglog_busn(PS1D_CDROM_SEEK, DBGLS_INFO, "(Cmd) Seek");
    stat_irq();
    io.stat.seek = 1;
    io.stat.read = 0;
    io.stat.play = 0;
    head.mode = HM_DATA;
    schedule_seek_finish(clock+seek_cycles());
}

void CDROM::cmd_motor_on(u64 clock) {
    printf("\n(CDROM) CMD MotorOn");
    stat_irq();
    motor.spinning_up = 1;
    schedule_finish(clock + (ONEFRAME * 60));
}

void CDROM::update_track_loc() {
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

void CDROM::queue_xa_sector(u8 *ptr) {
    if (xa_sector_buf.len > 2) {
        printf("\nWARN XA AUDIO UP TO 2 BUFFERS!");
    }
    if (xa_sector_buf.len >= 8) {
        printf("\nWARN DROP XA BUFFER!");
    }
    // Check filter to see if we should just discard
    if (io.MODE.xa_filter) {
        u8 filenum = ptr[XA_SUBHEADER_START+0];
        if (filenum != xa.filter.file) {
            printf("\nIGNORE XAPDCM FILE %02x not %02x!", filenum, xa.filter.file);
            return;
        }
        u8 chnum = ptr[XA_SUBHEADER_START+1];
        if (chnum != xa.filter.channel) {
            printf("\nIGNORE XADPCM CH %02x NOT %02x!", chnum, xa.filter.channel);
        }
        return;
    }
    memcpy(xa_sector_buf.bufs[xa_sector_buf.tail], ptr, 0x930);
    xa_sector_buf.tail = (xa_sector_buf.tail + 1) & 7;
    xa_sector_buf.len++;
}

void CDROM::read_sector() {
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

void CDROM::sch_read(u64 key, u64 clock) {
    read_sector();
    next_sector();
    schedule_read(clock);
}

void CDROM::next_sector() {
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

void CDROM::get_CD_audio(i16 &left, i16 &right) {
    left = right = 0;
    if (io.MODE.xa_adpcm) {
        if (io.stat.play) return;
        if (head.mode != HM_AUDIO) return;
        get_CD_audio_xa(left, right);
    }
    else {
        if (!io.stat.play) return;
        if (head.mode != HM_AUDIO) return;
        get_CD_audio_cdda(left, right);
    }
}

void CDROM::get_CD_audio_xa(i16 &left, i16 &right) {

}

void CDROM::get_CD_audio_cdda(i16 &left, i16 &right) {
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

void CDROM::cmd_motor_on_finish() {
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

void CDROM::cmd_stop(u64 clock) {
    printf("\n(CDROM) CMD Stop");
    io.stat.seek = 0;
    stat_irq();
    schedule_finish(clock + (ONEFRAME * 60 * 2));
}

void CDROM::cmd_pause(u64 clock) {
    //printf("\n(CDROM) CMD Pause");
    dbgloglog_busn(PS1D_CDROM_PAUSE, DBGLS_INFO, "(Cmd) Pause");
    stat_irq();
    if (read.still_sched) bus->scheduler.delete_if_exist(read.sched_id);
    io.stat.read = 0;
    schedule_finish(clock + ONEFRAME/60);
}

void CDROM::cmd_gettn() {
    // int3 stat,first,last
    result(io.stat.u);
    result(1);
    result(data.num_tracks);
    queue_interrupt(3);
    printf("\nGetTN NUMBER OF TRACKS: %d", data.num_tracks);
    finish_CMD(false, 0);
}

void CDROM::cmd_gettd() {
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

void CDROM::cmd_demute() {
    head.muted = false;
    finish_CMD(true, 3);

}

void CDROM::cmd_init(u64 clock) {
    //printf("\n(CDROM) CMD Init");
    stat_irq();
    io.MODE.u = 0x20;
    io.stat.motor_fullspeed = 1;
    schedule_finish(clock + UKN_TIME);
}

void CDROM::cmd_setmode() {
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

void CDROM::cmd_reset() {
    printf("\n(CDROM) CMD RESET.");
    finish_CMD(true, 3);
    sector_buf.head = sector_buf.tail = sector_buf.len = 0;
    io.interrupts.reset();
    io.PARAMETER.reset();
}

void CDROM::cmd_set_session(u64 clock) {
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

void CDROM::cmd_set_session_finish(u64 clock) {
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

//void CDROM::cmd_() {
//    printf("\n(CDROM) CMD . NOT IMPL!");
//}

static void scheduled_cmd_start(void *ptr, u64 key, u64 timecode, u32 jitter) {
    auto *th = static_cast<CDROM *>(ptr);
    th->cmd_start(key, timecode - jitter);
}

static void scheduled_read(void *ptr, u64 key, u64 timecode, u32 jitter) {
    auto *th = static_cast<CDROM *>(ptr);
    th->sch_read(key, timecode - jitter);
}

static void scheduled_cmd_end(void *ptr, u64 key, u64 timecode, u32 jitter) {
    auto *th = static_cast<CDROM *>(ptr);
    th->cmd_finish(key, timecode - jitter);
}

static void scheduled_cmd_step3(void *ptr, u64 key, u64 timecode, u32 jitter) {
    auto *th = static_cast<CDROM *>(ptr);
    th->cmd_step3(key, timecode - jitter);
}

void CDROM::schedule_read(u64 clock) {
    u64 div = io.MODE.speed ? 150 : 75;
    u64 tm = clock + ((ONEFRAME * 60) / div);
    read.sched_id = bus->scheduler.only_add_abs(tm, 0, this, &scheduled_read, &read.still_sched);
}

void CDROM::schedule_finish(u64 clock) {
    //printf("\nSCHEDULING END FOR %02x", io.CMD);
    CMD.sched_id = bus->scheduler.only_add_abs(clock, 0, this, &scheduled_cmd_end, &CMD.still_sched);
}

void CDROM::schedule_step_3(u64 clock) {
    CMD.sched_id = bus->scheduler.only_add_abs(clock, 0, this, &scheduled_cmd_step3, &CMD.still_sched);
}

i64 CDROM::seek_cycles() {
    return (ONEFRAME*24);
}

void CDROM::schedule_seek_finish(u64 clock) {
    CMD.sched_id = bus->scheduler.only_add_abs(clock, 0, this, &scheduled_cmd_end, &CMD.still_sched);
}

void CDROM::schedule_CMD() {

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

void CDROM::queue_interrupt(u32 level) {
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

void CDROM::stat_irq() {
    result(io.stat.u);
    queue_interrupt(3);
}

void CDROM::finish_CMD(bool do_stat_irq, u32 irq_num) {
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

void CDROM::cancel_CMD() {
    if (CMD.still_sched) {
        printf("\nCANCEL %02x", io.CMD);
        bus->scheduler.delete_if_exist(CMD.sched_id);
    }
    else {
        printf("\n(CDROM) CMD NOT SCHEDULED??");
    }
}

void CDROM::write_01(u32 val, u8 sz) {
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

void CDROM::remove_disc() {
    disk.inserted = false;
}

void CDROM::open_drive() {
    io.stat.shell_open = 1;
    io.stat.motor_fullspeed = 0;
}

void CDROM::close_drive() {
    io.stat.shell_open = 0;
}

void CDROM::insert_disc(multi_file_set &mfs) {
    printf("\nInserting PSX CDROM! DO THIS!!!");
    io.stat.shell_open = 0;
    data.parse_cue(mfs);
    disk.inserted = true;
}

void CDROM::update_IRQs() {
    u32 lvl = (io.HINTMSK.u & io.HINTSTS.u & 0b11111) != 0;
    set_irq_lvl(set_irq_ptr, lvl);
    //printf("\n(CDROM) IRQ update %d", lvl);
}

void CDROM::write_02(u32 val, u8 sz) {
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
void CDROM::queue_sector_RDDATA() {
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

void CDROM::write_03(u32 val, u8 sz) {
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
void CDROM::reset_decoder() {
    printf("\n(CDROM)  WARN  reset decoder!");
}

void CDROM::reset() {
    io.HSTS.u = 0b00001000;
}

}
