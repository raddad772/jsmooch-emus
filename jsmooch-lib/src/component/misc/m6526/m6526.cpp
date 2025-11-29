//
// Created by . on 11/29/25.
//

#include "m6526.h"

namespace M6526 {

u8 core::read(u8 addr, u8 old, bool has_effect) {
    addr &= 15;
    switch(addr) {
        case 0b0000: return read_PRA();
        case 0b0001: return read_PRB(has_effect);
        case 0b0010: return regs.DDRA;
        case 0b0011: return regs.DDRB;
        case 0b0100: return timerA.count & 0xFF;
        case 0b0101: return (timerA.count >> 8) & 0xFF;
        case 0b0110: return timerB.count & 0xFF;
        case 0b0111: return (timerB.count >> 8) & 0xFF;
        case 0b1000: return regs.TOD10;
        case 0b1001: return regs.TODSEC;
        case 0b1010: return regs.TODMIN;
        case 0b1011: return regs.TODHR;
        case 0b1100: return regs.SDR;
        case 0b1101: return read_ICR(has_effect);
        case 0b1110: return regs.CRA.u;
        case 0b1111: return regs.CRB.u;
    }
    return old;
}

u8 core::read_PRA() {
    const u8 out = (pins.PRA_out & regs.DDRA);
    const u8 in = (pins.PRA_in & ~regs.DDRA);
    return out | in;
}

u8 core::read_PRB(bool has_effect) {
    const u8 out = (pins.PRB_out & regs.DDRB);
    const u8 in = (pins.PRB_in & ~regs.DDRB);
    if (has_effect) {
        pins.PC = 1;
        PC_count = 1;
    }
    u8 v = out | in;
    if (regs.CRA.PBON) {
        v &= 0xBF;
        v |= timerA.out << 6;
    }
    if (regs.CRB.PBON) {
        v &= 0x7F;
        v |= timerB.out << 7;
    }
    return v;
}


void core::write(u8 addr, u8 val) {
    addr &= 15;
    switch(addr) {
        case 0b0000: write_PRA(val); return;
        case 0b0001: write_PRB(val); return;
        case 0b0010: write_DDRA(val); return;
        case 0b0011: write_DDRB(val); return;
        case 0b0100: timerA.latch = (timerA.latch & 0xFF00) | val; return;
        case 0b0101: timerA.latch = (timerA.latch & 0xFF) | (val << 8); return;
        case 0b0110: timerB.latch = (timerB.latch & 0xFF00) | val; return;
        case 0b0111: timerB.latch = (timerB.latch & 0xFF) | (val << 8); return;
        case 0b1000:
            if (regs.CRB.ALARM) regs.ALARM10 = val;
            else regs.TOD10 = val;
            return;
        case 0b1001:
            if (regs.CRB.ALARM) regs.ALARMSEC = val;
            else regs.TODSEC = val;
            return;
        case 0b1010:
            if (regs.CRB.ALARM) regs.ALARMMIN = val;
            else regs.TODMIN = val;
            return;
        case 0b1011:
            if (regs.CRB.ALARM) regs.ALARMHR = val;
            else regs.TODHR = val;
            return;
        case 0b1100: regs.SDR = val; return;
        case 0b1101: write_ICR(val); return;
        case 0b1110: write_CRA(val); return;
        case 0b1111: write_CRB(val); return;
    }
}

void core::write_CRA(u8 val) {
    regs.CRA.u = val;
    if (regs.CRA.LOAD) {
        timerA.count = timerA.latch;
    }
    regs.CRA.LOAD = 0;
}

void core::write_CRB(u8 val) {
    regs.CRB.u = val;
    if (regs.CRB.LOAD) {
        timerB.count = timerB.latch;
    }
    regs.CRB.LOAD = 0;
}

u8 core::read_ICR(bool has_effect) {
    // read DATA
    u8 v = regs.ICR_DATA.u;
    if (has_effect) {
        regs.ICR_DATA.u = 0;
        pins.IRQ = 0;
    }
    return v;
}

void core::write_ICR(u8 val) {
    // write MASK
    if (!(val & 0x80)) {
        // if bit7 = 0, clear any written with 1
        regs.ICR_MASK.u &= ~(val & 0x1F);
    }
    else {
        // if bit7 = 1, set any written with 1
        regs.ICR_MASK.u |= val & 0x1F;
    }
    update_IRQs();
}


void core::write_PRA(u8 val) {
    pins.PRA_out = val;
}

void core::write_PRB(u8 val) {
    pins.PRB_out = val;
    pins.PC = 1;
    PC_count = 1;
}

void core::write_DDRA(u8 val) {

}

void core::write_DDRB(u8 val) {

}


void core::reset() {

}

void core::tick_A() {
    if (!timerA.count--) {
        timerA.count = timerA.latch;
        if (regs.CRA.OUTMODE) // toggle mode
            timerA.out ^= 1;
        else { // pulse mode
            timerA.out = 1;
            timerA.out_count = 1;
        }
        regs.ICR_DATA.TA = 1;
        update_IRQs();
        regs.CRA.START = regs.CRA.RUNMODE ^ 1;
        if (regs.CRB.START && regs.CRB.INMODE > 1) {
            bool do_tick = regs.CRB.INMODE == 2 ? true : pins.CNT;
            if (do_tick) tick_B();
        }
    }
}

void core::tick_B() {
    if (!timerB.count--) {
        if (regs.CRB.OUTMODE)
            timerB.out ^= 1;
        else {
            timerB.out = 1;
            timerB.out_count = 1;
        }
        regs.ICR_DATA.TB = 1;
        update_IRQs();
        regs.CRB.START = regs.CRB.RUNMODE ^ 1;
    }
}

void core::update_IRQs() {
    if (regs.ICR_DATA.u & regs.ICR_MASK.u & 0x1F) regs.ICR_DATA.IR = 1;
    pins.IRQ = regs.ICR_DATA.IR;
}

void core::cycle() {
    // update PC pin which pulses for 1 clock
    pins.PC &= PC_count--;

    // Update FLAG bit
    if ((old_FLAG != 0) && (pins.FLAG)) {
        regs.ICR_DATA.FLG = 1;
        update_IRQs();
    }
    old_FLAG = pins.FLAG;

    bool cnt_happened = pins.CNT && !old_CNT;
    old_CNT = pins.CNT;

    if (regs.CRA.START) {
        // Determine if clock pulse should happen depending on mode
        bool do_tick = regs.CRA.INMODE ? true : cnt_happened;
        // then do it
        if (do_tick) tick_A();
    }
    if (regs.CRB.START) {
        // Determine if clock pulse should happen depending on mode
        bool do_tick = false;
        switch (regs.CRB.INMODE) {
            case 0: do_tick = true; break;
            case 1: do_tick = cnt_happened; break;
            case 2: // covered by tick_A()
            case 3: // covered by tick_A()
                break;
        }
        if (do_tick) tick_B();
    }

    if (!regs.CRA.OUTMODE) {
        timerA.out &= timerA.out_count--;
    }
    if (!regs.CRB.OUTMODE) {
        timerB.out &= timerB.out_count--;
    }

}

}