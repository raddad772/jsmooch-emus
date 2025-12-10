//
// Created by . on 12/3/25.
//

#include "mac_via.h"
#include "mac_bus.h"
#include "mac_regs.h"

namespace mac {

void via::update_RA()
{
    int8_t a = 4;
    i16 b = static_cast<i16>(a);
    // emulated: bits 4, 5, 6
    // not emulated:
    // bit 0-2, sound volume
    // bit 3, alternate sound buffer
    // bit 7, vSCCWReq, SCC wait/request

    u8 val = (regs.ORA & regs.dirA); // When a pin is programmed as an output, it's controlled by ORA.
    val |= (regs.IRA & (~regs.dirA));

    bus->iwm.write_HEADSEL((val >> 5) & 1);
    bus->io.ROM_overlay = (val >> 4) & 1;
}

void via::update_RB()
{
    // emulated:  bit 7 (sound), bits 0-2 (RTC), bit 6 (horizontal blank bit)
    // not emulated:
    // bits 4, 5 (mouse X2, Y2)
    // bit 3 (mouse switch)
    //u8 write_mask = regs.dirB;
    //if (write_mask & 0x80) {
    u8 val = (regs.ORB & regs.dirB); // When a pin is programmed as an output, it's controlled by ORA.

    bus->set_sound_output((val >> 7) & 1);
    bus->rtc.write_bits(val & 7, 7);
}


void via::write(u32 addr, u16 mask, u16 val)
{
    val >>= 8;
    u16 v = 0;
    if (addr < vBase) {
        printf("\nread BAD VIA WRITE ADDR? addr:%06x data:%02x cyc:%lld", addr, val, bus->clock.master_cycles);
        return;
    }
    addr -= vBase;
    switch(addr) {
        case vIFR: {// interrupt flag register
            u8 old_val = regs.IFR;
            //regs.IFR = ((val ^ 0xFF) & regs.IFR) & 0x7F;
            regs.IFR &= (~val) & 0x7F;
            //printf("\nWrite IFR %02x new:%02x old:%02x cyc:%lld", val, regs.IFR, old_val, clock.master_cycles);
            return; }
        case vIER: {// interrupt enable register
            u32 flags_to_affect = val & 0x7F;
            val &= 0x7F;
            // 00 set, 0x80 clear
            if (val & 0x80) { // Enable flags
                regs.IER |= val;
            }
            else { // Disable flags
                regs.IER &= ~val;
            }
            irq_sample();
            return; }
        case vDirA: // Direction reg A
            regs.dirA = val;
            update_RA();
            return;
        case vDirB: // Direction reg B
            regs.dirB = val;
            update_RB();
            return;
        case vT1C: // timer 1 count lo
            regs.T1L = (regs.T1L & 0xFF00) | val;
            return;
        case vT1CH:
            regs.IFR &= 0b10111111;
            irq_sample();

            regs.T1L = (regs.T1L | 0xFF) | (val << 8);

            regs.T1C = regs.T1L;
            state.t1_active = 1;
            if (((regs.ACR & regs.dirB) >> 7) & 1) {
                bus->set_sound_output(0);
            }
            return;
        case vT1L:
            regs.T1L = (regs.T1L & 0xFF00) | val;
            return;
        case vT1LH:
            regs.T1L = (regs.T1L | 0xFF) | (val << 8);
            return;
        case vT2C: // write timer 2 lo
            irq_sample();

            regs.T2L = (regs.T2L | 0xFF00) | val;
            return;
        case vT2CH: // write timer 2 hi
            regs.IFR &= 0b11011111;
            irq_sample();

            regs.T2L = (regs.T2L | 0xFF) | (val << 8);
            regs.T2C = regs.T2L;
            state.t2_active = 1;
            return;
        case vSR: // keyboard shfit reg
            regs.IFR &= 0b11111011;
            irq_sample();
            printf("\nWrite keyboard SR: %02x cyc:%lld", val, bus->clock.master_cycles);

            regs.SR = val;
            return;
        case vACR: // aux control reg
            // TODO: more
            regs.ACR = val;
            return;
        case vPCR: // peripheral control reg
            // TODO: more
             // bit 0 - vblank IRQ control
             // bit 1-3 - one-second interrupt control
            regs.PCR = val;
            return;
        case vBufA: {// Write Data Reg A

            regs.IFR &= 0b11111100;
            irq_sample();

            regs.ORA = val;
            update_RA();
            if ((regs.ORA & 0x20) != (val & 0x20)) {
                bus->iwm.lines.SELECT = (val >> 5) & 1;
                printf("\nFLOPPY HEADSEL line via Via A to: %d", (val >> 5) & 1);
            }

            //printf("\nwrite VIA BufA data:%02x cyc:%lld", val, bus->clock.master_cycles);
            return;}
        case vBufB: {// write Data Reg B
            regs.IFR &= 0b11100111;
            irq_sample();

            regs.ORB = val;

            update_RB();

            return;}
    }
    printf("\nUnhandled VIA write addr:%06x val:%04x cyc:%lld", addr, (val & mask) >> 8, bus->clock.master_cycles);
}
    
u16 via::read(u32 addr, u16 mask, u16 old, bool has_effect)
{
    u16 v = 0;
    if (addr < vBase) {
        printf("\nread BAD VIA ADDR? addr:%06x cyc:%lld", addr, bus->clock.master_cycles);
        return old;
    }
    addr -= vBase;
    switch(addr) {
        case vIFR: // interrupt flag register
            //dbg_printf("\npIFR: %02x", regs.IFR);
            v = (regs.IFR & 0x7F);// & regs.IER;
            v |= v ? 0x80 : 0;
            //dbg_printf("\nIFR: %02x", v);
            return v << 8;
        case vIER: // interrupt enable register
            return ((regs.IER & 0x7F) | 0x80) << 8;
        case vDirA: // Direction reg A
            // TODO: apply changes here
            return regs.dirA << 8;
        case vDirB: // Direction reg B
            // TODO: apply changes here
            return regs.dirB << 8;
        case vT1C: // timer 1 count lo
            regs.IFR &= 0b10111111;
            irq_sample();
            return (regs.T1C & 0xFF) << 8;
        case vT1CH:
            return (regs.T1C & 0xFF00);
        case vT1L:
            return (regs.T1L & 0xFF) << 8;
        case vT1LH:
            return (regs.T1L & 0xFF00);
        case vT2C: // read t2 lo
            regs.IFR &= 0b11011111;
            irq_sample();

            return (regs.T2C & 0xFF) << 8;
        case vT2CH: // read t2 hi
            return regs.T2C & 0xFF00;
        case vSR: // keyboard shfit reg
            regs.IFR &= 0b11111011;
            irq_sample();
            printf("\nREAD KEYBOARD SR! cyc:%lld", bus->clock.master_cycles);
            return regs.SR << 8;
        case vACR: // aux control reg
            return regs.ACR << 8;
        case vPCR: // peripheral control reg
            return regs.PCR << 8;
        case vBufA: {// read Data Reg A
            // not emulated
            // bit 7 - SCC wait/request
            regs.IFR &= 0b11111100;
            irq_sample();

            v = bus->io.ROM_overlay << 4;
            // For un-emulated bits, return the last thing we wrote
            v |= (regs.ORA & (~(1 << 4)));
            v |= 0x80; // SCC wait/request 1 so it doesn't keep just looping there
            return ((v << 8) | v) & mask; }
            //return regs.regA << 8; }
        case vBufB: {// read Data Reg B
            regs.IFR &= 0b11100111;
            irq_sample();
            v = 0;

            // First, inputs...
            u8 IRB_mask = ~regs.dirB;

            // Setup v with inputs
            v |= (bus->sound.io.on << 7) & IRB_mask;
            v |= (bus->display.in_hblank() << 6) & IRB_mask;
            v |= bus->rtc.read_bits() & IRB_mask; // bits 0-2

            // Now for the unemulated pins, fill with ORB no matter what
            u8 emulated_mask = 0b00111000;
            v |= regs.ORB & emulated_mask;

            // Now fill the rest with ORB, since these will be returned for output bits
            u8 ORB_mask = regs.dirB;
            v |= 8; // mouse button not pressed
            v |= regs.ORB & ORB_mask;
            return ((v << 8) | v) & mask; }
            /* The VIA event timers use the Enable signal (E clock) as a reference; therefore, the timer
counter is decremented once every 1.2766 uS. Timer T2 can be programmed to count
down once each time the VIA receives an input for bit 6 of Data register B. */
        default:
    }
    printf("\nUnhandled VIA read addr:%06x cyc:%lld", addr, bus->clock.master_cycles);
    return old;
}

void via::reset()
{
    state.t1_active = state.t2_active = 0;
}

void via::irq_sample()
{
    u32 old_irq = bus->io.irq.via;
    bus->io.irq.via = (regs.IER & regs.IFR & 0x7F) != 0;
    if (old_irq != bus->io.irq.via) bus->set_cpu_irq();
}

void via::step()
{
    irq_sample();

    // TODO: add .5 cycles to the countdowns
    u32 IRQ_sample = 0;
    if (state.t1_active) {
        // 7=PB out
        // 6=1 is continuous, 6=0 is one-shot
        if (regs.T1C == 0) {
            // PB7 gets modified if
            if (((regs.ACR & regs.dirB) >> 7) & 1) {
                bus->set_sound_output(1);
            }
            regs.IFR |= 0x40;
            IRQ_sample = 1;

            if ((regs.ACR >> 6) & 1) {
                regs.T1C = regs.T1L;
            }
            else {
                state.t1_active = 0;
            }
        }
        else regs.T1C--;
    }
    if (state.t2_active) {
        if (regs.T2C == 0) {
            regs.IFR |= 0x20;
            IRQ_sample = 1;
            state.t2_active = 0;
        }
    }
    regs.T2C--; // T2C counts down no matter what
    if (IRQ_sample) irq_sample();
}

    
}