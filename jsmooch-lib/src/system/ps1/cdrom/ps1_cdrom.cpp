//
// Created by . on 3/6/25.
//

#include "ps1_cdrom.h"

#include <stdnoreturn.h>

namespace PS1 {

static u32 constexpr masksz[5] = {0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF };

void CD_FIFO::reset() {
    head = tail = len = 0;
    memset(entries, 0, sizeof(entries));
}

void CD_FIFO::push(u32 val) {
    if (len == 16) {
        printf("\nATTEMPT TO PUSH TO FULL FIFO!");
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

u32 CDROM::read(u32 addr, u8 sz, bool has_effect) {
    switch (addr) {
        case 0x1F801800: {
            u32 v = io.HSTS.u | (io.HSTS.u << 8) | (io.HSTS.u << 16) | (io.HSTS.u << 24);
            return v & masksz[sz];
        }
        case 0x1F801801: return read_01(sz, has_effect);
        case 0x1F801802: return read_02(sz, has_effect);
        case 0x1F801803: return read_03(sz, has_effect);
        default:
    }
    NOGOHERE;
}

void CDROM::write(u32 addr, u32 val, u8 sz) {
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
        return v;
    }
    else return masksz[sz];
}

u32 CDROM::read_03(u8 sz, bool has_effect) {
    switch (io.HSTS.RA) {
        case 0:
        case 2: // HINTMSK
            return io.HINTMSK.u | 0b11100000;
        case 1:
        case 3: // HINTSTS
            return io.HINTSTS.u | 0b11100000;
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

void CDROM::result(u32 val) {
    io.RESULT.push(val);
}

void CDROM::cmd_start(u64 key, u64 clock) {
    u32 cmd = io.CMD;
    if ((cmd >= 0x20) && (cmd <= 0x4F)) cmd = 0;
    if (cmd >= 0x60) cmd = 0;
    switch(cmd) {
        case 0x00:
        case 0x17:
        case 0x18:
            queue_interrupt(5);
            result(0x11);
            result(0x40);
            finish_CMD();
            return;
        case 0x58:
        case 0x59:
        case 0x5A:
        case 0x5B:
        case 0x5C:
        case 0x5D:
        case 0x5E:
        case 0x5F:
            finish_CMD();
            return;
        case 0x01: // NOP
            queue_interrupt(3);
            result(io.stat.u);
            io.stat.shell_open = 0;
            finish_CMD();
            return;
        case 0x02: // SetLoc
            queue_interrupt(3);
            result(io.stat.u);
            cmd_setloc();
            return;
        case 0x03: // Play
            queue_interrupt(3);
            result(io.stat.u);
            cmd_play();
            return;
        case 0x04: // Forward
            queue_interrupt(3);
            result(io.stat.u);
            cmd_forward();
            return;
        case 0x05: // Backward
            queue_interrupt(3);
            result(io.stat.u);
            cmd_backward();
            return;
        case 0x06:
            queue_interrupt(3);
            result(io.stat.u);
            cmd_readn();
            return;
        case 0x07:
            queue_interrupt(3);
            result(io.stat.u);
            cmd_standby();
            return;
        case 0x08:
            queue_interrupt(3);
            result(io.stat.u);
            cmd_stop();
            return;
        case 0x09:
            queue_interrupt(3);
            result(io.stat.u);
            cmd_pause();
            return;
        case 0x0a:
            queue_interrupt(3);
            result(io.stat.u);
            cmd_init();
            return;


    }
    printf("\n\nUnhandled CDROM CMD %d\n", io.CMD);
}

void CDROM::cmd_setloc() {
    printf("\n(CDROM) CMD SetLoc. NOT IMPL!");
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

void CDROM::cmd_readn() {
    printf("\n(CDROM) CMD . NOT IMPL!");
}

void CDROM::cmd_standby() {
    printf("\n(CDROM) CMD ReadN. NOT IMPL!");
}

void CDROM::cmd_stop() {
    printf("\n(CDROM) CMD Stop. NOT IMPL!");
}

void CDROM::cmd_pause() {
    printf("\n(CDROM) CMD Pause. NOT IMPL!");
}

void CDROM::cmd_init() {
    printf("\n(CDROM) CMD Init. NOT IMPL!");
}

void CDROM::cmd_() {
    printf("\n(CDROM) CMD . NOT IMPL!");
}

void CDROM::cmd_() {
    printf("\n(CDROM) CMD . NOT IMPL!");
}

void CDROM::cmd_() {
    printf("\n(CDROM) CMD . NOT IMPL!");
}

void CDROM::cmd_() {
    printf("\n(CDROM) CMD . NOT IMPL!");
}

void CDROM::cmd_() {
    printf("\n(CDROM) CMD . NOT IMPL!");
}

static void scheduled_cmd_start(void *ptr, u64 key, u64 timecode, u32 jitter) {
    auto *th = static_cast<CDROM *>(ptr);
    th->cmd_start(key, timecode + jitter);
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
    if (io.HINTSTS.INTSTS == 0) {
        io.HINTSTS.INTSTS = level;
        update_IRQs();
    }
    else {
        io.interrupts.push(level);
    }
}

void CDROM::finish_CMD() {
    io.HSTS.BUSYSTS = 0;
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
            return;
        case 2: // CI
            io.CI.u = val & 0b01010101;
            // TODO: scheduler changes for sample rate?
            return;
        case 3: // ATV2 R->R
            io.R_R = val & 0xFF;;
            return;
    }
}


void CDROM::insert_disc(multi_file_set &mfs) {
    printf("\nInserting PSX CDROM! DO THIS!!!");
}

void CDROM::update_IRQs() {
    u32 lvl = io.HINTMSK.u & io.HINTSTS.u & 0b11111;
    set_irq_lvl(set_irq_ptr, lvl);
}

void CDROM::write_02(u32 val, u8 sz) {
    switch (io.HSTS.RA) {
        case 0: // PARAMETER
            io.PARAMETER.push(val);
            recalc_HSTS();
            return;
        case 1: {
            // HINTMSK
            u32 old = io.HINTMSK.u;
            io.HINTMSK.u = (val & 0b11111) | 0b11100000;
            if (io.HINTMSK.u != old) update_IRQs();
            return; }
        case 2: // ATV0 L->L
            io.L_L = val & 0xFF;
            return;
        case 3: // ATV3 R->L
            io.R_L = val & 0xFF;
            return;
    }
}

void CDROM::write_03(u32 val, u8 sz) {
    switch (io.HSTS.RA) {
        case 1: {
            // HCLRCTL
            u32 old = io.HINTMSK.u;
            io.HINTSTS.u &= ((val & 0b11111) ^ 0b11111);
            io.HINTSTS.u |= 0b11100000;
            if (io.HINTSTS.INTSTS == 0 && io.interrupts.len > 0) {
                io.HINTSTS.INTSTS = io.interrupts.pop();
            }
            if (old != io.HINTSTS.u) update_IRQs();
            if (val & 0x80) {
                reset_decoder();
            }
            if (val & 0x40) {
                io.PARAMETER.reset();
            }
             if (val & 0x20) {
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
