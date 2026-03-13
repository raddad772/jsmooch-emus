//
// Created by . on 3/6/25.
//

#include "helpers/debug.h"
#include "ps1_cdrom.h"
#include "../ps1_debugger.h"
#include "helpers/debugger/debugger.h"
#include "../ps1_bus.h"

namespace PS1::CDROM {
static constexpr u32 INVALID_ARG = 0x10;
static constexpr u32 INCORRECT_NUM_PARAMS = 0x20;
static constexpr u32 INVALID_COMMAND = 0x40;
static constexpr u32 NOT_READY = 0x80;

static u32 constexpr masksz[5] = {0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF };
#define CYCLES_PER_SEC 33868800
#define ONEFRAME 564480

#define cd_dbg_printf(...)

#define UKN_TIME ONEFRAME
#define TIME_IN_SEC static_cast<double>(bus->clock.master_cycle_count) / 33868800.0

    // no old, no latest: latest = this
    // old, no latest: latest = this
    // no old, latest: old = latest, latest = this
    // old, latest: latest = this
void SECTOR_FIFO::push(u8 *ptr, u32 num) {
    memcpy(data, ptr, num);
    len = num;
    pos = 0;
}

u32 SECTOR_FIFO::pop() {
    u32 p = pos;
    if (pos < len) pos++;
    return data[p];
}

void SECTOR_FIFO::clear() {
    pos = len = 0;
}

void FIFO16::reset() {
    head = tail = len = 0;
}

void FIFO16::push(u8 val) {
    entries[tail] = val;
    if (len >= 16) {
        printf("\nPUSH FULL FIFO16!");
        return;
    }
    tail = (tail + 1) & 15;
    len++;
}

u8 FIFO16::pop() {
    u8 o = entries[head];
    if (len) {
        head = (head + 1) & 15;
        len--;
    }
    return o;
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
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "READ HSTS %02x", io.HSTS.u);
            cd_dbg_printf("\nHSTS %02x", io.HSTS.u);
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
    u8 v = io.response.pop();
    cd_dbg_printf("\nRESULT %02x", v);
    dbgloglog_bus(PS1D_CDROM_RESULT, DBGLS_INFO, "POP RESULT %02x", v);
    return v;
}

u32 core::read_02(u8 sz, bool has_effect) {
    u32 v = io.buffered_data.pop();
    if (sz >= 2) v |= io.buffered_data.pop() << 8;
    if (sz == 4) {
        v |= io.buffered_data.pop() << 16;
        v |= io.buffered_data.pop() << 24;
    }
    if (io.buffered_data.pos >= io.buffered_data.len) {
        dbgloglog_busn(PS1D_CDROM_RDDATA_FINISH, DBGLS_TRACE, "RDDATA FIFO emptied!");
    }
    cd_dbg_printf("\nRD@%d: %08x", io.buffered_data.pos - 4, v);
    return v;
}

u32 core::read_03(u8 sz, bool has_effect) {
    u32 v;
    switch (io.HSTS.RA) {
        case 0:
        case 2: // HINTMSK
            v = irq.ready.enable;
            v |= irq.complete.enable << 1;
            v |= irq.ack.enable << 2;
            v |= irq.end.enable << 3;
            v |= irq.error.enable << 4;
            v |= 0b11100000;
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "READ HINTMSK %02x", v);
            return v;
        case 1:
        case 3: {
            // HINTSTS
            v = 0;
            if (irq.error.flag) v = 5;
            if (irq.end.flag) v = 4;
            if (irq.ack.flag) v = 3;
            if (irq.complete.flag) v = 2;
            if (irq.ready.flag) v = 1;
            v |= irq.end.flag << 3;
            v |= irq.error.flag << 4;
            v |= 0b11100000;
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "READ HINTSTS %02x", v);
            return v;
        }
    }
    NOGOHERE;
}

void core::recalc_HSTS() {
    io.HSTS.ADPBUSY = 0;
    io.HSTS.PRMEMPT = io.param.len == 0;
    io.HSTS.PRMWRDY = io.param.len != 16;
    io.HSTS.RSLRRDY = io.response.len > 0;
    io.HSTS.DRQSTS = io.HCHPCTL.BFRD && io.buffered_data.len > 0;
}

void core::write_CMD(u32 val) {
    // TODO: this
    if (io.HSTS.BUSYSTS) {
        printf("\n(CDROM) WARN IGNORE CMD %02x FOR CURRENT %02x", val, io.CMD);
        //cancel_CMD();
        return;
    }
    //printf("\n(CDROM) CMD WRITE %02x", val);
    io.CMD = val & 0xFF;
    io.HSTS.BUSYSTS = 1;
    schedule_CMD();
}

void core::result_error(u8 code) {
    result(5, {static_cast<u8>(io.stat.u | 1), code});
}

void core::cmd_start(u64 key, u64 clock) {
    u32 cmd = io.CMD;
    if ((cmd >= 0x20) && (cmd <= 0x4F)) cmd = 0;
    if (cmd >= 0x60) cmd = 0;
    cd_dbg_printf("\nCMD %02x", cmd);
    //printif(ps1.cdrom.cmd, "\n(CDROM) EXEC CMD %02x", io.CMD);
    if (bus->dbg.dvptr->ids_enabled[PS1D_CDROM_CMD]) {
        char cmdstr[256];
        char *ptr = cmdstr;
#define CDEF(n, x) case n: ptr += snprintf(cmdstr, sizeof(cmdstr), #x); break
        switch (cmd) {
            CDEF(0x0D, "SetFilter");
            CDEF(0x19, "Test");
            CDEF(0x01, "NOP");
            CDEF(0x02, "SetLoc");
            CDEF(0x03, "Play");
            CDEF(0x04, "Forward");
            CDEF(0x05, "Backward");
            CDEF(0x13, "GetTN");
            CDEF(0x14, "GetTD");
            CDEF(0x1B, "ReadS");
            CDEF(0x06, "ReadN");
            CDEF(0x07, "MotorOn");
            CDEF(0x08, "Stop");
            CDEF(0x09, "Pause");
            CDEF(0x0b, "Mute");
            CDEF(0x12, "SetSession");
            CDEF(0x0a, "Init");
            CDEF(0x0c, "Demute");
            CDEF(0x0e, "SetMode");
            CDEF(0x15, "SeekL");
            CDEF(0x11, "GetLocP");
            CDEF(0x16, "SeekP");
            CDEF(0x1a, "GetID");
            CDEF(0x1c, "Reset");
            default:
                ptr += snprintf(cmdstr, sizeof(cmdstr), "Unknown");
                break;
        }
        for (u32 i = 0; i < io.param.len; i++) {
            u32 e = (io.param.head + i) & 15;
            ptr += snprintf(ptr, sizeof(cmdstr) - (ptr - cmdstr), " %02x", int(io.param.entries[e]));
        }
        dbgloglog_bus(PS1D_CDROM_CMD, DBGLS_INFO, "CMD %02x: %s", io.CMD, cmdstr);
    }

    /*if ((cmd != 0x1B) && (cmd != 0x06)) {
        printf("\n(CDROM) CMD %02x", cmd);
    }*/
    switch(cmd) {
        case 0x0D: // setfilter
            cmd_setfilter();
            return;
        case 0x00:
        case 0x17:
        case 0x18:
            result_error(INVALID_COMMAND);
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
        case 0x0b:
            cmd_mute();
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
            result(5, { 0x06, 0x40} );
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
            io.stat.read = 0;
            io.stat.play = 0;
            io.stat.seek = 0;
            head.amm = 0;
            head.asect = 0;
            head.ass = 2;
            if (read.still_sched) bus->scheduler.delete_if_exist(read.sched_id);
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
            latch_seek();
            seek.last_read.disc.amm = head.amm;
            seek.last_read.disc.asect = head.asect;
            seek.last_read.disc.ass = head.ass;
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
    u8 sub = io.param.pop();
    test.subcmd = sub;
    //printf("\n(CDROM) TEST %02x", sub);
    switch (sub) {
        case 0x04: // Reset SCEx counters
            io.stat.motor_fullspeed = 1;
            finish_CMD(true, 3);
            return;
        case 0x05: // Read SCEx counters
            result(3, {io.stat.u, 0, 0});
            finish_CMD(false, 0);
            return;
        case 0x20: // CDROM BIOS version 94h,09h,19h,C0h
            result(3, {0x94, 0x09, 0x19, 0xC0});
            finish_CMD(false, 0);
            return;
        case 0x21: { // Status of POS0 and DOOR switches
            u8 v = head.sector == 0;
            v |= io.stat.shell_open << 1;
            result(3, {v});
            finish_CMD(false, 0);
            return; }
        case 0x22: // Not supported/higher BIOS version
            result_error(INVALID_ARG);
            finish_CMD(false, 0);
            return;
        case 0x25: // Not supported/higher BIOS version
            result_error(INVALID_ARG);
            return;
    }
    printf("\n(CDROM) TEST UNKNOWN CMD %02x", sub);
    result_error(INVALID_ARG);
}

void core::cmd_setfilter() {
    xa.filter.file = io.param.pop();
    xa.filter.channel = io.param.pop() & 0x1F; // TODO: ?
    finish_CMD(true, 3);
}

void core::latch_seek() {
    head.amm = seek.waiting.amm;
    head.ass = seek.waiting.ass;
    head.asect = seek.waiting.asect;
    dbgloglog_bus(PS1D_CDROM_SEEK, DBGLS_INFO, "(SEEK) Latching seek to %02d:%02d:%02d", head.amm, head.ass, head.asect);
}

void core::cmd_setloc() {
    u32 mm = io.param.pop();
    u32 ss = io.param.pop();
    u32 ff = io.param.pop();
    //printf("\n(CDROM) CMD SetLoc %02x:%02x:%02x", mm, ss, ff);
    if (((mm & 0x0F) > 0x09) || (mm > 0x99) || ((ss & 0x0F) > 0x09) || (ss >= 0x60) || ((ff & 0x0F) > 0x09) || (ff >= 0x75)){
        printf("\nBAD SETLOC!");
        result_error(INVALID_ARG);
        finish_CMD(false, 0);
        return;
    }

    stat_irq();
    seek.waiting.amm = decode_bcd(mm);
    seek.waiting.ass = decode_bcd(ss);
    seek.waiting.asect = decode_bcd(ff);
    seek.needs_seek = true;
    dbgloglog_bus(PS1D_CDROM_SETLOC, DBGLS_INFO, "(Cmd) SetLoc %02d:%02d:%02d", seek.waiting.amm, seek.waiting.ass, seek.waiting.asect);
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
    dbgloglog_busn(PS1D_CDROM_PLAY, DBGLS_INFO, "(Cmd) Play");
    if (io.stat.shell_open) {
        result_error(NOT_READY);
        finish_CMD(false, 0);
        return;
    }
    stat_irq();
    u32 track = io.param.len > 0 ? decode_bcd(io.param.pop()) : 0;
    if (track != 0) {
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
            head.asect = LBA % 75;
            head.ass = (LBA / 75) % 60;
            head.amm = (LBA / (75 * 60));
        }
    }
    io.stat.play = 1;
    finish_CMD(false, 0);
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
    stat_irq();
    if (read.still_sched && !seek.needs_seek) {
        dbgloglog_busn(PS1D_CDROM_READ, DBGLS_INFO, "(Cmd) ReadN/S ABORT for already going!");
        finish_CMD(false, 0);
        return;
    }

    if (seek.needs_seek) {
        dbgloglog_busn(PS1D_CDROM_READ, DBGLS_INFO, "(Cmd) ReadN/S: Seek processing adds time...");
    }
    // TODO: clear cmd second response

    io.stat.seek = seek.needs_seek;
    finish_CMD(false, 0);
    if (io.stat.seek) {
        clock += seek_cycles();
    }
    schedule_read(clock);
}

void core::cmd_getid(u64 clock) {
    //printf("\n(CDROM) CMD GetID");
    if (io.stat.shell_open) {
        printf("\nGetID not ready!");
        result_error(NOT_READY);
        finish_CMD(false, 0);
        return;
    }
    if (motor.spinning_up) {
        printf("\nGetID spinning up!");
        result_error(NOT_READY);
        finish_CMD(false, 0);
        return;
    }

    stat_irq();
    schedule_finish(clock + 0x4AA6);
}

void core::cmd_getid_finish() {
    if (!disk.inserted) {
        printf("\nGetID NO DISC!");
        result(5, {0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
        finish_CMD(false, 0);
        return;
    }
    result(2, {io.stat.u, 0x00, 0x20,0x00, 'S', 'C', 'E', 'A'});
    finish_CMD(false, 0);
}

void core::cmd_seekp(u64 clock) {
    printf("\n(CDROM) CMD SeekP");
    stat_irq();
    io.stat.seek = 1;
    io.stat.read = 0;
    io.stat.play = 0;
    head.mode = HM_AUDIO;
    schedule_seek_finish(clock+seek_cycles());
}

void core::cmd_getlocp() {
    // Within track
    update_track_loc();
    result(3, {
        encode_bcd(get_track_from_LBA(get_cur_LBA())), // track #
        1, // index
        encode_bcd(seek.last_read.track.amm), // track amm/ass/asect
        encode_bcd(seek.last_read.track.ass),
        encode_bcd(seek.last_read.track.asect),
        encode_bcd(seek.last_read.disc.amm), // global amm/ass/asect
        encode_bcd(seek.last_read.disc.ass),
        encode_bcd(seek.last_read.disc.asect),
    });
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
            //printf("\nIGNORE XADPCM FILE %02x not %02x!", filenum, xa.filter.file);
            return;
        }
        u8 chnum = ptr[XA_SUBHEADER_START+1];
        if (chnum != xa.filter.channel) {
            //printf("\nIGNORE XADPCM CH %02x NOT %02x!", chnum, xa.filter.channel);
            return;
        }
    }
    if (xa.sector_buf.len > 2) {
        printf("\nWARN XA AUDIO UP TO %d BUFFERS! FILTER:%d  ", xa.sector_buf.len, io.MODE.xa_filter);
        printf("\nXA MODE:%d", io.MODE.xa_adpcm);
    }
    if (xa.sector_buf.len >= 8) {
        printf("\nWARN DROP XA BUFFER!");
    }
    memcpy(xa.sector_buf.bufs[xa.sector_buf.tail], ptr, 0x930);
    xa.sector_buf.tail = (xa.sector_buf.tail + 1) & 7;
    xa.sector_buf.len++;
}

void core::read_sector() {
    io.stat.seek_error = 0;

    if (seek.needs_seek || io.stat.seek) {
        seek.needs_seek = false;
        io.stat.seek = 0;
        io.stat.read = 1;
        latch_seek();
    }
    seek.last_read.disc.amm = head.amm;
    seek.last_read.disc.asect = head.asect;
    seek.last_read.disc.ass = head.ass;


    u8 *ptr = data.ptr_to_data(head.amm, head.ass, head.asect);

    u8 subheader = ptr[18];
    u8 mode = ptr[15];
    u32 form = (subheader & 0x20) ? 2 : 1;
    if (mode == 2) {
        if (io.MODE.xa_filter) {
            if (ptr[16] != xa.filter.file) return;
            if (ptr[17] != xa.filter.channel) return;
        }
        if (io.MODE.xa_adpcm && ((subheader & 0x44) == 0x44)) {
            queue_xa_sector(ptr);
            return;
        }
        if (io.MODE.xa_filter && ((subheader & 0x44) == 0x44)) return;
    }
    //printf("\nMODE:%d SUBHEADER:%02x", mode, subheader);
    dbgloglog_bus(PS1D_CDROM_SECTOR_READS, DBGLS_INFO, "(READ) Read sector %02d:%02d:%02d  raw_sector:%d  LBA:%d  mode:%d  form:%d  subheader:%02x", head.amm, head.ass, head.asect, io.MODE.sector_size, get_cur_LBA(), mode, form, subheader);
    io.buffered_data.clear();
    /*
    // if (raw_sector) {
    // } else {
    //   if not mode1 or mode2, ignore with warning!
    //   offset = mode_1 ? 12 + 4, 12 + 12
    //   len = 0x800
    // }
    */
    cd_dbg_printf("\nRead sector %02d:%02d:%02d", head.amm, head.ass, head.asect);
    if (io.MODE.sector_size == 0) {
        io.buffered_data.push(ptr + 24, 0x800);
    }
    else {
        io.buffered_data.push(ptr + 12, 0x924);
    }

    result(1, {io.stat.u});
    bus->dma.channels[3].try_dreq();
}

void core::sch_read(u64 key, u64 clock) {
    read_sector();
    next_sector();
    schedule_read(clock);
}

void core::next_sector() {
    head.asect++;
    if (head.asect >= 75) {
        head.asect = 0;
        head.ass++;
        if (head.ass>=60) {
            head.ass = 0;
            head.amm++;
            if (head.amm >= 74) {
                head.amm = 0;
            }
        }
    }
}

void core::get_CD_audio(i16 &left, i16 &right) {
    left = right = 0;
    io.HSTS.ADPBUSY = 0;
    if (io.MODE.xa_adpcm) {
        get_CD_audio_xa(left, right);
    }
    else {
        if (!io.stat.play) return;
        if (head.mode != HM_AUDIO) return;
        get_CD_audio_cdda(left, right);
    }
    if (head.muted) {
        left = right = 0;
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

    static constexpr i32 filter_table_pos[16] = {0, 60, 115, 98, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    static constexpr i32 filter_table_neg[16] = {0, 0, -52, -55, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


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
    if (shift > 12) shift = 9;
    u8 filter = (hd >> 4) & 0x0F;
    const s32 filter_pos = filter_table_pos[filter];
    const s32 filter_neg = filter_table_neg[filter];

    //u32 sn = xa.decoder.samples.pos;
    for (u32 i = 0; i < 28; i++) {
        i32 t;
        u32 addr = (i*4)+blk;
        // t = signed4bit((src[16+blk+j*4] SHR (nibble*4)) AND 0Fh)
        // src[16+blk+j*4]
        // we already did 16+
        // so
        // src[blk+j*4]
        if (nibble == 0) t = ptr[addr] & 0x0F;
        else t = ptr[addr] >> 4;
        t = static_cast<i16>(t << 12);
        i32 s = t >> shift;
        i32 filtered = s + ((static_cast<i32>(old) * filter_pos) >> 6) + ((static_cast<i32>(older) * filter_neg) >> 6);
        if (filtered < -0x8000) filtered = -0x8000;
        if (filtered > 0x7FFF) filtered = 0x7FFF;
        *s_out = static_cast<i16>(filtered);
        s_out++;
        older = old;
        old = static_cast<i16>(filtered);
    }
}

    static constexpr i32 zzt[7][29] =       {{0,      0x0,     0x0,     0x0,    0x0,     -0x0002, 0x000A,  -0x0022, 0x0041, -0x0054,
            0x0034, 0x0009,  -0x010A, 0x0400, -0x0A78, 0x234C,  0x6794,  -0x1780, 0x0BCD, -0x0623,
            0x0350, -0x016D, 0x006B,  0x000A, -0x0010, 0x0011,  -0x0008, 0x0003,  -0x0001},
           {0,       0x0,    0x0,     -0x0002, 0x0,    0x0003,  -0x0013, 0x003C,  -0x004B, 0x00A2,
            -0x00E3, 0x0132, -0x0043, -0x0267, 0x0C9D, 0x74BB,  -0x11B4, 0x09B8,  -0x05BF, 0x0372,
            -0x01A8, 0x00A6, -0x001B, 0x0005,  0x0006, -0x0008, 0x0003,  -0x0001, 0x0},
           {0,      0x0,     -0x0001, 0x0003,  -0x0002, -0x0005, 0x001F,  -0x004A, 0x00B3, -0x0192,
            0x02B1, -0x039E, 0x04F8,  -0x05A6, 0x7939,  -0x05A6, 0x04F8,  -0x039E, 0x02B1, -0x0192,
            0x00B3, -0x004A, 0x001F,  -0x0005, -0x0002, 0x0003,  -0x0001, 0x0,     0x0},
           {0,       -0x0001, 0x0003,  -0x0008, 0x0006, 0x0005,  -0x001B, 0x00A6, -0x01A8, 0x0372,
            -0x05BF, 0x09B8,  -0x11B4, 0x74BB,  0x0C9D, -0x0267, -0x0043, 0x0132, -0x00E3, 0x00A2,
            -0x004B, 0x003C,  -0x0013, 0x0003,  0x0,    -0x0002, 0x0,     0x0,    0x0},
           {-0x0001, 0x0003,  -0x0008, 0x0011,  -0x0010, 0x000A, 0x006B,  -0x016D, 0x0350, -0x0623,
            0x0BCD,  -0x1780, 0x6794,  0x234C,  -0x0A78, 0x0400, -0x010A, 0x0009,  0x0034, -0x0054,
            0x0041,  -0x0022, 0x000A,  -0x0001, 0x0,     0x0001, 0x0,     0x0,     0x0},
           {0x0002,  -0x0008, 0x0010,  -0x0023, 0x002B, 0x001A,  -0x00EB, 0x027B,  -0x0548, 0x0AFA,
            -0x16FA, 0x53E0,  0x3C07,  -0x1249, 0x080E, -0x0347, 0x015B,  -0x0044, -0x0017, 0x0046,
            -0x0023, 0x0011,  -0x0005, 0x0,     0x0,    0x0,     0x0,     0x0,     0x0},
           {-0x0005, 0x0011,  -0x0023, 0x0046, -0x0017, -0x0044, 0x015B,  -0x0347, 0x080E, -0x1249,
            0x3C07,  0x53E0,  -0x16FA, 0x0AFA, -0x0548, 0x027B,  -0x00EB, 0x001A,  0x002B, -0x0023,
            0x0010,  -0x0008, 0x0002,  0x0,    0x0,     0x0,     0x0,     0x0,     0x0}};

bool core::xa_decode_next_sector() {
    u8 CI;
    u8 *dat = xa_get_sector(CI);
    if (dat == nullptr) {
        return false;
    }
    auto channels = static_cast<XA::XA_CH>(CI & 3);
    auto sample_rate = static_cast<XA::XA_SR>((CI >> 2) & 3);
    auto bps = static_cast<XA::XA_BPS>((CI >> 4) & 3);
    //printf("\nXA SECTOR CH:%d SR:%d BPS:%d", channels == XA::STEREO ? 2 : 1, sample_rate == XA::SR37800 ? 37800 : 18900, bps == XA::BITS4 ? 4 : 8);
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
    u32 sample_pos = 0;
    for (u32 block_num = 0; block_num < 0x12; block_num++) {
        // 4 copies...
        dat += 4;
        // 8 headers
        memcpy(headers, dat, 8);
        // advance past 8 + 4 more copies...
        dat += 12;

        u8 hdr = 0;
        for (u32 blk = 0; blk < 4; blk++) {
            if (channels == XA::STEREO) {
                xa_decode_28(dat, blk, 0, headers[hdr++], xa.decoder.l_old, xa.decoder.l_older, xa.decoder.samples.l + sample_pos);
                xa_decode_28(dat, blk, 1, headers[hdr++],  xa.decoder.r_old, xa.decoder.r_older, xa.decoder.samples.r + sample_pos);
            }
            else {
                xa_decode_28(dat, blk, 0, headers[hdr++], xa.decoder.l_old, xa.decoder.l_older, xa.decoder.samples.l + sample_pos);
                sample_pos += 28;
                xa_decode_28(dat, blk, 1, headers[hdr++], xa.decoder.l_old, xa.decoder.l_older, xa.decoder.samples.l + sample_pos);
            }
            // advance 28 (or 28+28) ADPCM samples
            sample_pos += 28;
            // advance 28 bytes
        } // finish 4 blocks
        // get out of this block
        dat += 112;
    } // finish 18 block-of-blocks

    xa.decoder.samples.len = sample_pos;

    // Now zigzag it!
    u32 s_index=0;
    u32 out_index=0;
    u32 num_loops = xa.decoder.samples.len / 6;
    for (u32 i = 0; i < num_loops; i++) {
        // First we need to output 6 samples
        for (u32 u = 0; u < 3; u++) { // 3 * 2 = 6 samples @ 38700
            // push 2 samples 3 times
            // either 2 samples, or 1 repeated sample for half-rate
            if (sample_rate == XA::SR37800) {
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
                xa.decoder.ringbuf.l[xa.decoder.ringbuf.pos+1] = xa.decoder.samples.l[s_index];;
                if (channels == XA::STEREO) {
                    xa.decoder.ringbuf.r[xa.decoder.ringbuf.pos] = xa.decoder.samples.l[s_index];
                    xa.decoder.ringbuf.r[xa.decoder.ringbuf.pos+1] = xa.decoder.samples.l[s_index];
                }
                s_index++;
            }
            xa.decoder.ringbuf.pos = (xa.decoder.ringbuf.pos + 2) & 0x1F;
        }
        // Now that we've filled 6 samples (or 6 interp. @ 37800), out 7 44.1kHz!
        for (u32 tblnum = 0; tblnum < 7; tblnum++) {
            xa.decoder.out_samples.l[out_index] = zigzaginterp(xa.decoder.ringbuf.l, xa.decoder.ringbuf.pos, zzt[tblnum]);
            if (channels == XA::STEREO) {
                xa.decoder.out_samples.r[out_index] = zigzaginterp(xa.decoder.ringbuf.r, xa.decoder.ringbuf.pos, zzt[tblnum]);
            }
            else {
                xa.decoder.out_samples.r[out_index] = xa.decoder.out_samples.l[out_index];
            }
            out_index++;
        }
    }
    xa.decoder.out_samples.len = out_index;
    xa.decoder.out_samples.pos = 0;
    return true;
}

void core::get_CD_audio_xa(i16 &left, i16 &right) {
    if (xa.decoder.out_samples.pos >= xa.decoder.out_samples.len) {
        bool yo = xa.decoder.out_samples.len > 0;
        if (!xa_decode_next_sector()) {
            return;
        }
    }
    io.HSTS.ADPBUSY = 1;

    // Advance in our current block!
    left = xa.decoder.out_samples.l[xa.decoder.out_samples.pos];
    right = xa.decoder.out_samples.r[xa.decoder.out_samples.pos];
    xa.decoder.out_samples.pos++;
}

i16 core::zigzaginterp(const i16 *ringbuf, u32 p, const i32 *tabl) {
    i32 sum = 0;
    for (u32 i = 0; i < 29; i++) {
        sum +=(static_cast<i32>(ringbuf[(p-i) & 0x1F]) * static_cast<i32>(tabl[i])) >> 15;
    }
    if (sum < -0x8000) sum = -0x8000;
    if (sum > 0x7FFF) sum = 0x7FFF;
    return static_cast<i16>(sum);
}

void core::get_CD_audio_cdda(i16 &left, i16 &right) {
    //u32 addr = head.sample_index * 4;
    u32 LBA = head.asect + (head.ass * 75) + (head.amm * 75 * 50);

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
    /*if (io.stat.motor_fullspeed) {
        // Give error INT5!
        result(5, {static_cast<u8>(io.stat.u | 1), INVALID_COMMAND});
    }
    else {*/
        io.stat.motor_fullspeed = 1;
        motor.spinning_up = 0;
        result(2, {io.stat.u});
    //}
    finish_CMD(false, 3);
}

void core::cmd_stop(u64 clock) {
    printf("\n(CDROM) CMD Stop");
    io.stat.seek = 0;
    io.stat.read = 0;
    io.stat.play = 0;
    stat_irq();
    if (read.still_sched) bus->scheduler.delete_if_exist(read.sched_id);
    schedule_finish(clock + (ONEFRAME * 60 * 2));
}

void core::cmd_mute() {
    head.muted= true;
    finish_CMD(true, 3);
}

void core::cmd_pause(u64 clock) {
    //printf("\n(CDROM) CMD Pause");
    dbgloglog_busn(PS1D_CDROM_PAUSE, DBGLS_INFO, "(Cmd) Pause");
    stat_irq();
    io.stat.read = 0;
    if (read.still_sched) bus->scheduler.delete_if_exist(read.sched_id);
    schedule_finish(clock + ONEFRAME);
}

void core::cmd_gettn() {
    // int3 stat,first,last
    result(3, {io.stat.u, 1, static_cast<u8>(data.num_tracks)} );
    printf("\nGetTN NUMBER OF TRACKS: %d", data.num_tracks);
    finish_CMD(false, 0);
}

void core::cmd_gettd() {
    if (io.param.len != 1) {
        result_error(INCORRECT_NUM_PARAMS);
        finish_CMD(false, 0);
        return;
    }
    u32 track_num = decode_bcd(io.param.pop());
    //printf("\n(CDROM) CMD GetTD %d", track_num);
    if (track_num > data.num_tracks) {
        result_error(INVALID_ARG);
        finish_CMD(false, 0);
        return;
    }
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
    result(3, {io.stat.u, encode_bcd(mm), encode_bcd(ss)});
    finish_CMD(false, 0);
}

void core::cmd_demute() {
    head.muted = false;
    finish_CMD(true, 3);

}

void core::cmd_init(u64 clock) {
    //printf("\n(CDROM) CMD Init");
    io.MODE.u = 0x20;
    result(3, {io.stat.u});
    io.stat.read = 0;
    io.stat.motor_fullspeed = 1;
    io.stat.shell_open = !disk.inserted;
    schedule_finish(clock + UKN_TIME);
}

void core::cmd_setmode() {
    u8 mode = io.param.pop();
    //printf("\n(CDROM) CMD SETMODE %02x", mode);
    dbgloglog_bus(PS1D_CDROM_READ, DBGLS_INFO, "(Cmd) SetMode %02x", mode);
    //io.latch.MODE_sector_size = (mode >> 5) & 1;
    if (!(mode & 0x10)) {
        //io.latch.MODE_sector_size = (mode >> 5) & 1;
    }
    if (((mode >> 7) & 1) != io.MODE.speed) {
        speed_changed = 20321280;
    }
    io.MODE.u = mode;
    finish_CMD(true, 3);
}

void core::cmd_reset() {
    printf("\n(CDROM) CMD RESET!?");
    finish_CMD(true, 3);
    /*buffered
    sector_buf.reset();
    io.irq1s.reset();
    io.irqnot1s.reset();
    io.results_in.reset();
    io.results_out.reset();
    if (read.still_sched) {
        bus->scheduler.delete_if_exist(read.sched_id);
    }
    if (CMD.still_sched) {
        bus->scheduler.delete_if_exist(CMD.sched_id);
    }

    io.PARAMETER.reset();*/
}

void core::cmd_set_session(u64 clock) {
    //printf("\n(CDROM) CMD SetSession");
    seek.session = io.param.pop();
    if (seek.session == 0) {
        result_error(INVALID_ARG);
        finish_CMD(false, 0);
        return;
    }
    if (io.stat.play || io.stat.read) {
        result_error(NOT_READY);
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
            result(5, {0x06, 0x40});
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
            result_error(INCORRECT_NUM_PARAMS);
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

    i64 r = (ONEFRAME*20);

    r += speed_changed;
    speed_changed = 0;
    //r = 30000; // "max" speedup from duckstation
    return r;
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

void core::result(u32 level, std::initializer_list<u8> rdata) {
    //printf("\n(CDROM) QUEUE INT %d", level);
    // if there's no IRQ, AND
    //. ((the IRQ isn't a level 1),
    //.   or, if it *IS* a level 1,
    //.   there are no non-level1 IRQs
    //    and there are no in-process commands

    if (!irq.pending()) {
        cd_dbg_printf("\nAssert IRQ %d", level);
        for (auto & n : rdata) cd_dbg_printf(" %02x", n);
        dbgloglog_bus(PS1D_CDROM_IRQ_ASSERT, DBGLS_TRACE, "IRQ Assert %d", level);
        io.response.reset();
        for (auto & n : rdata) io.response.push(n);
        switch (level) {
            case 1:
                irq.ready.flag = true; break;
            case 2:
                irq.complete.flag = true; break;
            case 3:
                irq.ack.flag = true; break;
            case 4:
                irq.end.flag = true; break;
            case 5:
                irq.error.flag = true; break;
            default:
                NOGOHERE;
        }
        update_IRQs();
        return;
    }
    dbgloglog_bus(PS1D_CDROM_IRQ_ASSERT, DBGLS_TRACE, "IRQ Queue %d", level);
    switch (level) {
        case 1: // Ready data only happens if there isn't already one
            if (irq.deferred.ready.kind == 0) {
                irq.deferred.ready.kind = level;
                irq.deferred.ready.data.reset();
                for (auto & n : rdata) irq.deferred.ready.data.push(n);
            }
            break;
        case 2: // Complete always overwrites what's there
            irq.deferred.complete.kind = level;
            irq.deferred.complete.data.reset();
            for (auto & n : rdata) irq.deferred.complete.data.push(n);
            break;
        case 3: // ack
        case 4: // end
        case 5: // error - any of these 3 all overwrite the "ack"
            irq.deferred.ack.kind = level;
            irq.deferred.ack.data.reset();
            for (auto &n : rdata) irq.deferred.ack.data.push(n);
            break;
    }
}

void core::stat_irq() {
    result(3, {io.stat.u});
}

void core::finish_CMD(bool do_stat_irq, u32 irq_num) {
    io.HSTS.BUSYSTS = 0;
    io.param.reset();
    //printif(ps1.cdrom.cmd, "\n(CDROM) CMD FINISH");
    //printf("\nfinish_CMD %02x", io.CMD);

    if (do_stat_irq) {
        result(irq_num, {io.stat.u});
    }
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
    u32 lvl = irq.any_can_fire();
    set_irq_lvl(set_irq_ptr, lvl);
    //printf("\n(CDROM) IRQ update %d", lvl);
}

void core::write_02(u32 val, u8 sz) {
    switch (io.HSTS.RA) {
        case 0: // PARAMETER
            if (io.param.len < 16) {
                dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "WRITE PARAMETER %02x", val);
                io.param.push(val & 0xFF);
                return;
            }
            printf("\nCDROM drive PARAM overflow!");
            return;
        case 1: {
            // HINTMSK
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "WRITE HINTMASK %02x", val);
            irq.ready.enable = val & 1;
            irq.complete.enable = (val >> 1) & 1;
            irq.ack.enable = (val >> 2) & 1;
            irq.end.enable = (val >> 3) & 1;
            irq.error.enable = (val >> 4) & 1;
            update_IRQs();
            return; }
        case 2: // ATV0 L->L
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "WRITE L->L %02x", val);
            io.L_L = val & 0xFF;
            return;
        case 3: // ATV3 R->L
            io.R_L = val & 0xFF;
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "WRITE R->L %02x", val);
            return;
    }
}

bool core::deliver_deferred(IRQSTRUCT::DEFERREDSTRUCT::DEFERREDSTRUCTITEM &item) {
    if (item.kind == 0) return false;
    io.response.reset();
    for (u32 i = 0; i < item.data.len; i++) io.response.push(item.data.pop());
    item.data.reset();

    u8 t = item.kind;
    item.kind = 0;

    switch (t) {
        case 1: irq.ready.flag = true; break;
        case 2: irq.complete.flag = true; break;
        case 3: irq.ack.flag = true; break;
        case 4: irq.end.flag = true; break;
        case 5: irq.error.flag = true; break;
    }
    dbgloglog_bus(PS1D_CDROM_IRQ_ASSERT, DBGLS_TRACE, "IRQ Deferred Assert %d", t);

    update_IRQs();
    return true;
}

void core::flush_deferred_IRQs() {
    if (irq.pending()) return;
    if (deliver_deferred(irq.deferred.ack)) return;
    if (deliver_deferred(irq.deferred.complete)) return;
    if (deliver_deferred(irq.deferred.ready)) return;
}

void core::write_03(u32 val, u8 sz) {
    switch (io.HSTS.RA) {
        case 0:
            io.HCHPCTL.u = val & 0b11100000;
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "WRITE HCHPCTL %02x/%02x", val,io.HCHPCTL.u);
            if (io.HCHPCTL.BFRD) {
                dbgloglog_busn(PS1D_CDROM_RDDATA_START, DBGLS_TRACE, "RDDATA REQUEST!");
            }
            bus->dma.channels[3].try_dreq();
            return;
        case 1: {
            // HCLRCTL
            dbgloglog_bus(PS1D_CDROM_REGRW, DBGLS_INFO, "WRITE HCLRCTL %02x", val);
            if ((val & 7) == 7) {
                if (irq.ready.flag) irq.ready.flag = false;
                else if (irq.complete.flag) irq.complete.flag = false;
                else if (irq.ack.flag) irq.ack.flag = false;
                else if (irq.end.flag) irq.end.flag = false;
                else if (irq.error.flag) irq.error.flag = false;
            }
            if (val & 8) irq.end.flag = false;
            if (val & 0x10) irq.error.flag = false;
            if (val & 040) io.param.reset();
            update_IRQs();
            flush_deferred_IRQs();
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
    io.stat.motor_fullspeed = 1;
}

}
