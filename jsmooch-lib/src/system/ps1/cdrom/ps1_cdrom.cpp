//
// Created by . on 3/6/25.
//

#include "helpers/debug.h"
#include "ps1_cdrom.h"

namespace PS1 {

static u32 constexpr masksz[5] = {0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF };
#define ONEFRAME (33868800/60)
#define UKN_TIME ONEFRAME

void CD_FIFO::reset() {
    head = tail = len = 0;
    memset(entries, 0, sizeof(entries));
}

void CD_FIFO::push(u32 val) {
    if (len == 16) {
        printf("\n(CDROM) ATTEMPT TO PUSH TO FULL FIFO!");
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
            io.HSTS.PRMEMPT = io.PARAMETER.len == 0;
            io.HSTS.PRMWRDY = io.PARAMETER.len != 16;
            io.HSTS.RSLRRDY = io.RESULT.len > 0;
            u32 v = io.HSTS.u | (io.HSTS.u << 8) | (io.HSTS.u << 16) | (io.HSTS.u << 24);
            printif(ps1.cdrom.reg_read, "\n(CDROM) read HSTS %02x", v & 0xFF);
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
        case 0x1F801800: io.HSTS.RA = val & 3; return;
        case 0x1F801801: return write_01(val, sz);
        case 0x1F801802: return write_02(val, sz);
        case 0x1F801803: return write_03(val, sz);
        default:
    }
    NOGOHERE;

}

u32 CDROM::read_01(u8 sz, bool has_effect) {
    // RESULT
    u8 v = io.RESULT.pop();
    recalc_HSTS();
    printif(ps1.cdrom.result_read, "\n(CDROM) read RESULT %02x", v);
    return v;
}

u32 CDROM::read_02(u8 sz, bool has_effect) {
    // RDDATA
    // if DRQSTS is set, the datablock (disk sector) can be then read from this register
    if (io.HSTS.DRQSTS) {
        u32 v = io.RDDATA.read_byte();
        if (sz >= 2) v |= io.RDDATA.read_byte() << 8;
        if (sz == 4) {
            v |= io.RDDATA.read_byte() << 16;
            v |= io.RDDATA.read_byte() << 24;
        }
        if (io.RDDATA.pos >= io.RDDATA.len) {
            //printf("\n\nFINISH RDDATA!");
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
            printif(ps1.cdrom.reg_read, "\n(CDROM) read HINTMSK %02x", v);
            return v;
        case 1:
        case 3: // HINTSTS
            v = io.HINTSTS.u | 0b11100000;
            printif(ps1.cdrom.reg_read, "\n(CDROM) read HINTSTS %02x", v);
            return v;
    }
    NOGOHERE;
}

void CDROM::recalc_HSTS() {
    io.HSTS.PRMEMPT = io.PARAMETER.len == 0;
    io.HSTS.PRMWRDY = io.PARAMETER.len != 16;
    io.HSTS.RSLRRDY = io.RESULT.len > 0;
}

void CDROM::write_CMD(u32 val) {
    // TODO: this
    if (io.HSTS.BUSYSTS) cancel_CMD();
    io.CMD = val & 0xFF;
    schedule_CMD();
}
void CDROM::result_string(const char *s) {
    for (u32 i = 0; i < strlen(s); i++) {
        if (s[i] != 0) result(s[i]);
    }
}

void CDROM::result(u32 val) {
    io.RESULT.push(val);
}

void CDROM::cmd_start(u64 key, u64 clock) {
    io.RESULT.reset();
    u32 cmd = io.CMD;
    if ((cmd >= 0x20) && (cmd <= 0x4F)) cmd = 0;
    if (cmd >= 0x60) cmd = 0;
    printif(ps1.cdrom.cmd, "\n(CDROM) EXEC CMD %02x", io.CMD);
    switch(cmd) {
        case 0x00:
        case 0x17:
        case 0x18:
            queue_interrupt(5);
            result(0x11);
            result(0x40);
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
        case 0x06: // ReadN
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
        case 0x0D: // setfilter
            ADPCM.filter.file = io.PARAMETER.pop();
            ADPCM.filter.channel = io.PARAMETER.pop();
            finish_CMD(true, 3);
            return;
        case 0x0e:
            cmd_setmode();
            return;
        case 0x15: // SeekL
            cmd_seekl(clock);
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
            queue_interrupt(5);
            result(0x06);
            result(0x40);
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
            finish_CMD(true, 2);
            return;
        case 0x0A: // Init
            finish_CMD(true, 2);
            return;
        case 0x12: // SetSession
            cmd_set_session_finish(clock);
            return;
        case 0x15: // SeekL
            io.stat.seek = 0;
            finish_CMD(true, 2);
            return;
        case 0x16: // SeekP
            io.stat.seek = 0;
            finish_CMD(true, 2);
            return;
        case 0x1A: // GetID
            cmd_getid_finish();
            return;
        case 0x06: // ReadN
        case 0x1B: // ReadS
            schedule_read(clock);
            return;
        default:
            NOGOHERE;
    }
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

void CDROM::cmd_setloc() {
    //printf("\n(CDROM) CMD SetLoc");
    stat_irq();
    seek.amm = decode_bcd(io.PARAMETER.pop());
    seek.ass = decode_bcd(io.PARAMETER.pop());
    seek.asect = decode_bcd(io.PARAMETER.pop());
    finish_CMD(false, 0);
}

void CDROM::cmd_play() {
    printf("\n(CDROM) CMD Play. NOT IMPL!");
}

void CDROM::cmd_forward() {
    printf("\n(CDROM) CMD Forward. NOT IMPL!");
}

void CDROM::cmd_backward() {
    printf("\n(CDROM) CMD Backward. NOT IMPL!");
}

void CDROM::cmd_reads(u64 clock) {
    //printf("\n(CDROM) CMD ReadS");
    // Read data!
    do_cmd_read(clock);
}

void CDROM::do_cmd_read(u64 clock) {
    if (read.still_sched) {
        printf("\nABORT read req. processing for already going!");
        stat_irq();
        return;
    }
    stat_irq();
    io.stat.seek = 0;
    io.stat.read = 1;
    schedule_finish(clock + ONEFRAME);
}

void CDROM::cmd_getid(u64 clock) {
    //printf("\n(CDROM) CMD GetID");
    if (io.stat.shell_open) {
        queue_interrupt(5);
        result(0x11);
        result(0x80);
        finish_CMD(false, 0);
        return;
    }
    if (motor.spinning_up) {
        queue_interrupt(5);
        result(0x01);
        result(0x80);
        finish_CMD(false, 0);
        return;
    }

    stat_irq();
    schedule_finish(clock + 0x4AA6);
}

void CDROM::cmd_getid_finish() {
    if (!disk.inserted) {
        queue_interrupt(3);
        result(0x08);
        result(0x40);
        result(0x00);
        result(0x00);
        result(0x00);
        result(0x00);
        result(0x00);
        result(0x00);
        finish_CMD(false, 0);
        return;
    }
    queue_interrupt(2);
    result(0x02);
    result(0x00);
    result(0x20);
    result(0x00);
    result_string("SCEA");
    finish_CMD(false, 0);
}

void CDROM::cmd_seekp(u64 clock) {
    printf("\n(CDROM) CMD SeekP");
    stat_irq();
    io.stat.seek = 1;
    io.stat.read = 0;
    io.stat.play = 0;
    head.mode = HM_AUDIO;
    schedule_seek_finish(clock);
}

void CDROM::cmd_seekl(u64 clock) {
    //printf("\n(CDROM) CMD SeekL");
    stat_irq();
    io.stat.seek = 1;
    io.stat.read = 0;
    head.mode = HM_DATA;
    schedule_seek_finish(clock);
}

void CDROM::cmd_motor_on(u64 clock) {
    printf("\n(CDROM) CMD MotorOn");
    stat_irq();
    motor.spinning_up = 1;
    schedule_finish(clock + (ONEFRAME * 60));
}

void CDROM::read_sector() {
    //printf("\n(CDROM) Read sector %02d:%02d:%02d", seek.amm, seek.ass, seek.asect);
    u8 *ptr = data.ptr_to_data(seek.amm, seek.ass, seek.asect);
    memcpy(sector_buf.bufs[sector_buf.tail], ptr, 0x930);
    sector_buf.tail = (sector_buf.tail + 1) & 7;
    sector_buf.len++;
    if (sector_buf.len > 2) {
        printf("\nWARN SECTOR BUF LEN %d", sector_buf.len);
    }
    queue_interrupt(1);
    result(io.stat.u);
}

void CDROM::sch_read(u64 key, u64 clock) {
    read_sector();
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
    schedule_read(clock);
}

void CDROM::cmd_motor_on_finish() {
    if (io.stat.motor_fullspeed) {
        // Give error INT5!
        queue_interrupt(5);
        result(io.stat.u);
        result(0x20);
    }
    else {
        io.stat.motor_fullspeed = 1;
        motor.spinning_up = 0;
        queue_interrupt(2);
        result(io.stat.u);
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
    stat_irq();
    if (read.still_sched) scheduler->delete_if_exist(read.sched_id);
    io.stat.read = 0;
    schedule_finish(clock + ONEFRAME);
}

void CDROM::cmd_gettn() {
    // int3 stat,first,last
    queue_interrupt(3);
    result(io.stat.u);
    result(1);
    result(data.num_tracks+1);
    finish_CMD(false, 0);
}

void CDROM::cmd_gettd() {
    u32 track_num = io.PARAMETER.pop();
    track_num--;
    queue_interrupt(3);
    if (track_num >= data.num_tracks) {
        result(0x10);
        finish_CMD(false, 0);
        return;
    }
    queue_interrupt(3);
    result(io.stat.u);
    // mm, ss
    u32 calc = data.tracks[track_num].data_lba;
    // ((mm * 60) + ss) * 75) + sect
    static constexpr u32 min_val = 60 * 75;
    static constexpr u32 sec_val = 75;
    u32 mm = calc / min_val;
    u32 ss = (calc % min_val) / sec_val;
    result(mm);
    result(ss);
    finish_CMD(false, 0);
}

void CDROM::cmd_demute() {
    printf("\n(CDROM) CMD Demute. NOT IMPL!");
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
    if (!(mode & 0x10)) {
        io.latch.MODE_sector_size = (mode >> 5) & 1;
    }
    io.MODE.u = mode;
    finish_CMD(true, 3);
}

void CDROM::cmd_reset() {
    //printf("\n(CDROM) CMD RESET.");
    finish_CMD(true, 3);
    sector_buf.head = sector_buf.tail = sector_buf.len = 0;
}

void CDROM::cmd_set_session(u64 clock) {
    //printf("\n(CDROM) CMD SetSession");
    seek.session = io.PARAMETER.pop();
    if (seek.session == 0) {
        queue_interrupt(5);
        result(0x03);
        result(0x10);
        finish_CMD(false, 0);
        return;
    }
    if (io.stat.play || io.stat.read) {
        queue_interrupt(5);
        result(0x80);
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
            queue_interrupt(5);
            result(0x06);
            result(0x40);
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
            queue_interrupt(5);
            result(0x06);
            result(0x20);
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
    read.sched_id = scheduler->only_add_abs(tm, 0, this, &scheduled_read, &read.still_sched);
}

void CDROM::schedule_finish(u64 clock) {
    CMD.sched_id = scheduler->only_add_abs(clock, 0, this, &scheduled_cmd_end, &CMD.still_sched);
}

void CDROM::schedule_step_3(u64 clock) {
    CMD.sched_id = scheduler->only_add_abs(clock, 0, this, &scheduled_cmd_step3, &CMD.still_sched);
}


void CDROM::schedule_seek_finish(u64 clock) {
    CMD.sched_id = scheduler->only_add_abs(clock + ONEFRAME, 0, this, &scheduled_cmd_end, &CMD.still_sched);
}

void CDROM::schedule_CMD() {

#define SC(y) CMD.sched_id = scheduler->only_add_abs(scheduler->current_time() + y, 0, this, &scheduled_cmd_start, &CMD.still_sched)
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
        update_IRQs();
    }
    else {
        io.interrupts.push(level);
    }
}

void CDROM::stat_irq() {
    queue_interrupt(3);
    result(io.stat.u);
}

void CDROM::finish_CMD(bool do_stat_irq, u32 irq_num) {
    io.HSTS.BUSYSTS = 0;
    //printif(ps1.cdrom.cmd, "\n(CDROM) CMD FINISH");
    if (do_stat_irq) {
        queue_interrupt(irq_num);
        result(io.stat.u);
    }
}

void CDROM::cancel_CMD() {
    if (CMD.still_sched) {
        scheduler->delete_if_exist(CMD.sched_id);
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
            //printf("\n(CDROM) CI write %02x", io.CI.u);
            // TODO: scheduler changes for sample rate?
            return;
        case 3: // ATV2 R->R
            io.R_R = val & 0xFF;;
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
            io.PARAMETER.push(val);
            recalc_HSTS();
            return;
        case 1: {
            // HINTMSK
            u32 old = io.HINTMSK.u;
            io.HINTMSK.u = (val & 0b11111) | 0b11100000;
            if (io.HINTMSK.u != old) update_IRQs();
            printif(ps1.cdrom.reg_write, "\n(CDROM) HINTMSK write %02x. new HINTMSK %02x", val, io.HINTMSK.u);
            return; }
        case 2: // ATV0 L->L
            io.L_L = val & 0xFF;
            return;
        case 3: // ATV3 R->L
            io.R_L = val & 0xFF;
            return;
    }
}
void CDROM::queue_sector_RDDATA() {
    if (sector_buf.len == 0) {
        printf("\nNO BUFS TO RDDATA!?");
        return;
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
}

void CDROM::write_03(u32 val, u8 sz) {
    switch (io.HSTS.RA) {
        case 0:
            io.read_mode = (val >> 7) & 1;
            //printf("\n(CDROM) write read mode %d", io.read_mode);
            if (!io.read_mode) {
                io.HSTS.DRQSTS = 0;
                io.RDDATA.clear();
            }
            else {
                io.HSTS.DRQSTS = sector_buf.len > 0;
                if (io.HSTS.DRQSTS) {
                    //printf("\n(CDROM) Queiueng read data!");
                    queue_sector_RDDATA();
                }
            }
            return;
        case 1: {
            // HCLRCTL
            u32 old = io.HINTMSK.u;
            io.HINTSTS.u &= ((val & 0b11111) ^ 0b11111);
            io.HINTSTS.u |= 0b11100000;
            //printf("\n(CDROM) reset RESULTs on HCLRCTL write");
            if (io.HINTSTS.INTSTS == 0 && io.interrupts.len > 0) {
                //printf("\n(CDROM) Queue next IRQ!");
                io.HINTSTS.INTSTS = io.interrupts.pop();
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
            return;
        case 3: // ADPCTL
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

void CDROM::ack_IRQ() {
    // Drain RESULT FIFO
    io.RESULT.reset();
    // If there's a pending command, send it!

}

void CDROM::reset() {
    io.HSTS.u = 0b00001000;
}

}
