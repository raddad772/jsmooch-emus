//
// Created by . on 7/24/24.
//

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "mac_internal.h"
#include "mac_display.h"

static u16 ulmask[4] = {
        0, // UDS = 0 LDS = 0
        0xFF, // UDS = 0 LDS = 1
        0xFF00, // UDS = 1 LDS = 0
        0xFFFF // UDS = 1 LDS = 1
};

static u8 read_rtc_bits(struct mac* this)
{
    u8 o = 0;
    o |= (this->rtc.tx.shift >> 7) & 1;
    o |= this->rtc.old_clock_bit << 1;
    o |= (this->rtc.tx.kind > 0) << 2;
    return o;
}


static void rtc_do_read(struct mac* this)
{
    u8 rand = this->rtc.cmd & 0b01110000;
    u8 addr;
    switch(rand) {
        case 0b00100000:
            addr = ((this->rtc.cmd >> 2) & 3) + 0x10;
            this->rtc.tx.shift = this->rtc.param_mem[addr];
            break;
        case 0b01000000:
            addr = (this->rtc.cmd >> 2) & 0x0F;
            this->rtc.tx.shift = this->rtc.param_mem[addr];
            break;
        default: {
            switch (this->rtc.cmd) {
                case 0b10000001:
                    this->rtc.tx.shift = this->rtc.seconds & 0xFF;
                    break;
                case 0b10000101:
                    this->rtc.tx.shift = (this->rtc.seconds >> 8) & 0xFF;
                    break;
                case 0b10001001:
                    this->rtc.tx.shift = (this->rtc.seconds >> 16) & 0xFF;
                    break;
                case 0b10001101:
                    this->rtc.tx.shift = (this->rtc.seconds >> 24) & 0xFF;
                    break;
                default:
                    printf("\nUnknown RTC command %02x", this->rtc.cmd);
                    break;
            }
        }
    }
}

static void rtc_finish_write(struct mac* this)
{
    u8 rand = this->rtc.cmd & 0b01110000;
    u8 addr;
    switch(rand) {
        case 0b00100000:
            addr = ((this->rtc.cmd >> 2) & 3) + 0x10;
            this->rtc.tx.shift = this->rtc.param_mem[addr];
            break;
        case 0b01000000:
            addr = (this->rtc.cmd >> 2) & 0x0F;
            this->rtc.tx.shift = this->rtc.param_mem[addr];
            break;
        default: {
            switch (this->rtc.cmd) {
                case 0b10000001:
                    this->rtc.seconds = (this->rtc.seconds & 0xFFFFFF00) | this->rtc.tx.shift;
                    break;
                case 0b10000101:
                    this->rtc.seconds = (this->rtc.seconds & 0xFFFF00FF) | (this->rtc.tx.shift << 8);
                    break;
                case 0b10001001:
                    this->rtc.seconds = (this->rtc.seconds & 0xFF00FFFF) | (this->rtc.tx.shift << 16);
                    break;
                case 0b10001101:
                    this->rtc.seconds = (this->rtc.seconds & 0xFFFFFF) | (this->rtc.tx.shift << 24);
                    break;
                case 0b00110001:
                    this->rtc.test_register = this->rtc.tx.shift;
                    printf("\nWrite RTC test reg %02x", this->rtc.tx.shift);
                    break;
                case 0b00110101:
                    this->rtc.write_protect_register = this->rtc.tx.shift;
                    break;
                default:
                    printf("\nUnknown RTC command %02x", this->rtc.cmd);
                    break;
            }
        }
    }
}

static void write_rtc_bits(struct mac* this, u8 val, u8 write_mask)
{
    // bit 0 - serial data
    // bit 1 - data-clock
    // bit 2 - serial enable (if 1, no data will be shifted, transaction cancelled)
    u8 data, clock, enable;
    if (write_mask & 1) data = val & 1;
    else data = (this->rtc.tx.shift & 0x80) >> 7;

    if (write_mask & 2) clock = (val >> 1) & 1;
    else clock = this->rtc.old_clock_bit;

    if (write_mask & 4) enable = ((val >> 2) & 1) ^ 1;
    else enable = this->rtc.tx.kind > 0;

    // transfers are 8 bits. write is 8 bits out + 8 bits outs
    // read is 8 bits out + 8 bits in
    if (enable) {
        if (clock != this->rtc.old_clock_bit) {
            this->rtc.old_clock_bit = clock;
            if (clock == 1) {
                // Do a transfer.
                // 1 = write, 2 = read
                if ((this->rtc.tx.progress < 8) || (this->rtc.tx.kind == 1)) {
                    // It will be a transfer in
                    this->rtc.tx.shift >>= 1;
                    this->rtc.tx.shift |= (data << 7);
                }
                else { // progress > 8 and it's a read
                    if (this->rtc.tx.kind == 1) {
                        this->rtc.tx.shift <<= 1;
                    }
                }
                this->rtc.tx.progress++;
                // We completed a command...
                if (this->rtc.tx.progress == 8) {
                    this->rtc.cmd = this->rtc.tx.shift;
                    if (this->rtc.cmd & 0x80) {
                        this->rtc.tx.kind = 1; // write
                    }
                    else {
                        this->rtc.tx.kind = 2; // read
                        rtc_do_read(this);
                        // get the data...
                    }
                }
                else if (this->rtc.tx.progress == 16) {
                    // We compelted totality
                    if (this->rtc.tx.kind == 1) { // read
                    }
                    else if (this->rtc.tx.kind == 2) { // write
                        // determine where to commit
                        if (this->rtc.write_protect_register & 0x80) {
                            // If write-protect on, only write to write-protect.
                            if (this->rtc.cmd != 0b00110101) {
                                printf("\nTried to write to write-protected RTC! cyc:%lld", this->clock.master_cycles);
                            }
                            else {
                                this->rtc.write_protect_register = this->rtc.tx.shift;
                            }
                        }
                        else {
                            rtc_finish_write(this);
                        }
                    }
                    else {
                        printf("\nREACHED END OF UNKNOWN TRANSFER? %02x cyc:%lld", this->rtc.cmd, this->clock.master_cycles);
                    }
                    this->rtc.tx.kind = 0;
                    this->rtc.tx.shift = 0;
                    this->rtc.tx.progress = 0;
                }
            }
        }
    }
    else {
        // reset transfer
        this->rtc.tx.shift = 0;
        this->rtc.tx.progress = 0;
        this->rtc.tx.kind = 0;
    }
}

void mac_set_sound_output(struct mac* this, u32 set_to)
{
    this->sound.io.on = set_to;
}

void mac_step_CPU(struct mac* this)
{
    //if (this->io.m68k.stuck) dbg_printf("\nSTUCK cyc %lld", *this->cpu.trace.cycles);

    M68k_cycle(&this->cpu);
    if (this->cpu.pins.FC == 7) {
        // Auto-vector interrupts!
        this->cpu.pins.VPA = 1;
        return;
    }
    if (this->cpu.pins.AS && (!this->cpu.pins.DTACK)) {
        if (!this->cpu.pins.RW) { // read
            this->io.cpu.last_read_data = this->cpu.pins.D = mac_mainbus_read(this, this->cpu.pins.Addr, this->cpu.pins.UDS, this->cpu.pins.LDS, this->io.cpu.last_read_data, 1);
            this->cpu.pins.DTACK = 1;
            if (dbg.trace_on && dbg.traces.m68000.mem && ((this->cpu.pins.FC & 1) || dbg.traces.m68000.ifetch)) {
                dbg_printf(DBGC_READ "\nr.M68k(%lld)   %06x  v:%04x" DBGC_RST, this->clock.master_cycles, this->cpu.pins.Addr, this->cpu.pins.D);
            }
        }
        else { // write
            mac_mainbus_write(this, this->cpu.pins.Addr, this->cpu.pins.UDS, this->cpu.pins.LDS, this->cpu.pins.D);
            this->cpu.pins.DTACK = 1;
            if (dbg.trace_on && dbg.traces.m68000.mem && (this->cpu.pins.FC & 1)) {
                dbg_printf(DBGC_WRITE "\nw.M68k(%lld)   %06x  v:%04x" DBGC_RST, this->clock.master_cycles, this->cpu.pins.Addr, this->cpu.pins.D);
            }
        }
    }
}

void mac_reset_via(struct mac* this)
{
    this->via.state.t1_active = this->via.state.t2_active = 0;
}

void mac_set_cpu_irq(struct mac* this)
{
    //M68k_set_interrupt_level(&this->cpu, this->io.irq.via | (this->io.irq.scc << 1) | (this->io.irq.iswitch << 2));
    M68k_set_interrupt_level(&this->cpu, 0);
}

void mac_via_irq_sample(struct mac* this)
{
    u32 old_irq = this->io.irq.via;
    this->io.irq.via = (this->via.regs.IER & this->via.regs.IFR & 0x7F) ? 1 : 0;
    if (old_irq != this->io.irq.via) mac_set_cpu_irq(this);
}

void step_via(struct mac* this)
{
    mac_via_irq_sample(this);

    // TODO: add .5 cycles to the countdowns
    u32 IRQ_sample = 0;
    if (this->via.state.t1_active) {
        // 7=PB out
        // 6=1 is continuous, 6=0 is one-shot
        if (this->via.regs.T1C == 0) {
            // PB7 gets modified if
            if (((this->via.regs.ACR & this->via.regs.dirB) >> 7) & 1) {
                mac_set_sound_output(this, 1);
            }
            this->via.regs.IFR |= 0x40;
            IRQ_sample = 1;

            if ((this->via.regs.ACR >> 6) & 1) {
                this->via.regs.T1C = this->via.regs.T1L;
            }
            else {
                this->via.state.t1_active = 0;
            }
        }
        else this->via.regs.T1C--;
    }
    if (this->via.state.t2_active) {
        if (this->via.regs.T2C == 0) {
            this->via.regs.IFR |= 0x20;
            IRQ_sample = 1;
            this->via.state.t2_active = 0;
        }
    }
    this->via.regs.T2C--; // T2C counts down no matter what
    if (IRQ_sample) mac_via_irq_sample(this);
}

void mac_step_eclock(struct mac* this)
{
    if (this->io.eclock == 0) {
        step_via(this);
    }

    this->io.eclock = (this->io.eclock + 1) % 10;
    if (this->io.eclock == 0) { // my RTC works off the e clock for some reason
        this->rtc.cycle_counter++;
        if (this->rtc.cycle_counter >= 783360) {
            this->rtc.cycle_counter = 0;
            this->rtc.seconds++;
            this->via.regs.IFR |= 1;
        }
    }

    mac_iwm_clock(this);
}

void mac_step_bus(struct mac* this)
{
    // Step everything in the mac by 2
    mac_step_CPU(this);         // CPU, 1 clock
    mac_step_display2(this);    // Display, 2 clocks
    mac_step_eclock(this);      // e-clock devices like VIA, 1 clocks, but only fires once every 10
    this->clock.master_cycles += 2;
}

#define iwm_dBase 0xDFE1FF
#define iwm_ph0L (0*512)  // CA0 off (0)
#define iwm_ph0H (1*512)  // CA0 on (1)
#define iwm_ph1L (2*512)  // CA1 off (0)
#define iwm_ph1H (3*512)  // CA1 on (1)
#define iwm_ph2L (4*512)  // CA2 off (0)
#define iwm_ph2H (5*512)  // CA2 on (1)
#define iwm_ph3L (6*512)  // LSTRB off (low)
#define iwm_ph3H (7*512)  // LSTRB on (high)
#define iwm_mtrOff (8*512) // disk enable off
#define iwm_mtrOn (9*512)  // disk enable on
#define iwm_intDrive (10*512) // select internal drive
#define iwm_extDrive (11*512) // select external drive
#define iwm_q6L (12*512) // Q6 off
#define iwm_q6H (13*512) // Q6 on
#define iwm_q7L (14*512) // Q7 off
#define iwm_q7H (15*512) // Q7 on

#define sccRBase  0x9FFFF8
#define sccWBase  0xBFFFF9
#define scc_aData 6
#define scc_aCtl 2
#define scc_bData 4
#define scc_bCtl 0

#define vBase  0xEFE1FE
#define aVBufB vBase    // via register B
#define aVBufA 0xEFFFFE // via register A
#define aVBufM aVBufB    // mouse buffer
#define aVIFR 0xEFFBFE   // interrupt flag
#define aVIER 0xEFFDFE   // interrupt enable

// offsets from vBase
#define vBufB (512*0)  // data reg b
#define vDirB (512*2)  // direction reg b
#define vDirA (512*3)  // direction reg a
#define vT1C  (512*4)  // timer 1 count lo
#define vT1CH (512*5)  // timer 1 count hi, write causes timer start
#define vT1L  (512*6)  // timer 1 latch lo
#define vT1LH (512*7)  // timer 1 latch hi
#define vT2C  (512*8)  // timer 2 count lo
#define vT2CH (512*9)  // timer 2 count hi
#define vSR   (512*10) // shift register, keyboard
#define vACR  (512*11) // aux control register
#define vPCR  (512*12) // peripheral control register
#define vIFR  (512*13) // interrupt flag register
#define vIER  (512*14) // interrupt enable register
#define vBufA (512*15)


u16 mac_mainbus_read_via(struct mac* this, u32 addr, u16 mask, u16 old, u32 has_effect)
{
    u16 v = 0;
    if (addr < vBase) {
        printf("\nread BAD VIA ADDR? addr:%06x cyc:%lld", addr, this->clock.master_cycles);
        return old;
    }
    addr -= vBase;
    switch(addr) {
        case vIFR: // interrupt flag register
            //dbg_printf("\npIFR: %02x", this->via.regs.IFR);
            v = (this->via.regs.IFR & 0x7F);// & this->via.regs.IER;
            v |= v ? 0x80 : 0;
            //dbg_printf("\nIFR: %02x", v);
            return v << 8;
        case vIER: // interrupt enable register
            return ((this->via.regs.IER & 0x7F) | 0x80) << 8;
        case vDirA: // Direction reg A
            // TODO: apply changes here
            return this->via.regs.dirA << 8;
        case vDirB: // Direction reg B
            // TODO: apply changes here
            return this->via.regs.dirB << 8;
        case vT1C: // timer 1 count lo
            this->via.regs.IFR &= 0b10111111;
            mac_via_irq_sample(this);
            return (this->via.regs.T1C & 0xFF) << 8;
        case vT1CH:
            return (this->via.regs.T1C & 0xFF00);
        case vT1L:
            return (this->via.regs.T1L & 0xFF) << 8;
        case vT1LH:
            return (this->via.regs.T1L & 0xFF00);
        case vT2C: // read t2 lo
            this->via.regs.IFR &= 0b11011111;
            mac_via_irq_sample(this);

            return (this->via.regs.T2C & 0xFF) << 8;
        case vT2CH: // read t2 hi
            return this->via.regs.T2C & 0xFF00;
        case vSR: // keyboard shfit reg
            this->via.regs.IFR &= 0b11111011;
            mac_via_irq_sample(this);
            printf("\nREAD KEYBOARD SR! cyc:%lld", this->clock.master_cycles);
            return this->via.regs.SR << 8;
        case vACR: // aux control reg
            return this->via.regs.ACR << 8;
        case vPCR: // peripheral control reg
            return this->via.regs.PCR << 8;
        case vBufA: {// read Data Reg A
            // not emulated
            // bit 7 - SCC wait/request
            this->via.regs.IFR &= 0b11111100;
            mac_via_irq_sample(this);

            v = this->io.ROM_overlay << 4;
            // For un-emulated bits, return the last thing we wrote
            v |= (this->via.regs.ORA & (~(1 << 4)));
            v |= 0x80; // SCC wait/request 1 so it doesn't keep just looping there
            return ((v << 8) | v) & mask; }
            //return this->via.regs.regA << 8; }
        case vBufB: {// read Data Reg B
            this->via.regs.IFR &= 0b11100111;
            mac_via_irq_sample(this);
            v = 0;

            // First, inputs...
            u8 IRB_mask = ~this->via.regs.dirB;

            // Setup v with inputs
            v |= (this->sound.io.on << 7) & IRB_mask;
            v |= (mac_display_in_hblank(this) << 6) & IRB_mask;
            v |= read_rtc_bits(this) & IRB_mask; // bits 0-2

            // Now for the unemulated pins, fill with ORB no matter what
            u8 emulated_mask = 0b00111000;
            v |= this->via.regs.ORB & emulated_mask;

            // Now fill the rest with ORB, since these will be returned for output bits
            u8 ORB_mask = this->via.regs.dirB;
            v |= 8; // mouse button not pressed
            v |= this->via.regs.ORB & ORB_mask;
            return ((v << 8) | v) & mask; }
            /* The VIA event timers use the Enable signal (E clock) as a reference; therefore, the timer
counter is decremented once every 1.2766 uS. Timer T2 can be programmed to count
down once each time the VIA receives an input for bit 6 of Data register B. */
    }
    printf("\nUnhandled VIA read addr:%06x cyc:%lld", addr, this->clock.master_cycles);
    return old;
}

void mac_clock_init(struct mac* this)
{
    this->clock.timing.cycles_per_frame = 704 * 370 * 60; // not quite right
}

void mac_via_load_SR(struct mac* this, u8 bit)
{
    this->via.regs.num_shifts++;
    this->via.regs.SR = (this->via.regs.SR >> 1) | ((bit & 1) << 7);
    if (this->via.regs.num_shifts == 8) {
        this->via.regs.num_shifts = 0;
        this->via.regs.IFR |= 4;
        mac_via_irq_sample(this);
    }
}

static void update_via_RA(struct mac* this)
{
    // emulated: bits 4, 6
    // not emulated:
    // bit 0-2, sound volume
    // bit 3, alternate sound buffer
    // bit 7, vSCCWReq, SCC wait/request

    u8 val = (this->via.regs.ORA & this->via.regs.dirA); // When a pin is programmed as an output, it's controlled by ORA.
    val |= (this->via.regs.IRA & (~this->via.regs.dirA));

    this->iwm.lines.SELECT = (val >> 5) & 1;
    this->io.ROM_overlay = (val >> 4) & 1;
}

static void update_via_RB(struct mac* this)
{
    // emulated:  bit 7 (sound), bits 0-2 (RTC), bit 6 (horizontal blank bit)
    // not emulated:
    // bits 4, 5 (mouse X2, Y2)
    // bit 3 (mouse switch)
    //u8 write_mask = this->via.regs.dirB;
    //if (write_mask & 0x80) {
    u8 val = (this->via.regs.ORB & this->via.regs.dirB); // When a pin is programmed as an output, it's controlled by ORA.

    mac_set_sound_output(this, (val >> 7) & 1);
    write_rtc_bits(this, val & 7, 7);
}

void mac_mainbus_write_via(struct mac* this, u32 addr, u16 mask, u16 val)
{
    val >>= 8;
    u16 v = 0;
    if (addr < vBase) {
        printf("\nread BAD VIA WRITE ADDR? addr:%06x data:%02x cyc:%lld", addr, val, this->clock.master_cycles);
        return;
    }
    addr -= vBase;
    switch(addr) {
        case vIFR: {// interrupt flag register
            u8 old_val = this->via.regs.IFR;
            //this->via.regs.IFR = ((val ^ 0xFF) & this->via.regs.IFR) & 0x7F;
            this->via.regs.IFR &= (~val) & 0x7F;
            //printf("\nWrite IFR %02x new:%02x old:%02x cyc:%lld", val, this->via.regs.IFR, old_val, this->clock.master_cycles);
            return; }
        case vIER: {// interrupt enable register
            u32 flags_to_affect = val & 0x7F;
            val &= 0x7F;
            // 00 set, 0x80 clear
            if (val & 0x80) { // Enable flags
                this->via.regs.IER |= val;
            }
            else { // Disable flags
                this->via.regs.IER &= ~val;
            }
            mac_via_irq_sample(this);
            return; }
        case vDirA: // Direction reg A
            this->via.regs.dirA = val;
            update_via_RA(this);
            return;
        case vDirB: // Direction reg B
            this->via.regs.dirB = val;
            update_via_RB(this);
            return;
        case vT1C: // timer 1 count lo
            this->via.regs.T1L = (this->via.regs.T1L & 0xFF00) | val;
            return;
        case vT1CH:
            this->via.regs.IFR &= 0b10111111;
            mac_via_irq_sample(this);

            this->via.regs.T1L = (this->via.regs.T1L | 0xFF) | (val << 8);

            this->via.regs.T1C = this->via.regs.T1L;
            this->via.state.t1_active = 1;
            if (((this->via.regs.ACR & this->via.regs.dirB) >> 7) & 1) {
                mac_set_sound_output(this, 0);
            }
            return;
        case vT1L:
            this->via.regs.T1L = (this->via.regs.T1L & 0xFF00) | val;
            return;
        case vT1LH:
            this->via.regs.T1L = (this->via.regs.T1L | 0xFF) | (val << 8);
            return;
        case vT2C: // write timer 2 lo
            mac_via_irq_sample(this);

            this->via.regs.T2L = (this->via.regs.T2L | 0xFF00) | val;
            return;
        case vT2CH: // write timer 2 hi
            this->via.regs.IFR &= 0b11011111;
            mac_via_irq_sample(this);

            this->via.regs.T2L = (this->via.regs.T2L | 0xFF) | (val << 8);
            this->via.regs.T2C = this->via.regs.T2L;
            this->via.state.t2_active = 1;
            return;
        case vSR: // keyboard shfit reg
            this->via.regs.IFR &= 0b11111011;
            mac_via_irq_sample(this);
            printf("\nWrite keyboard SR: %02x cyc:%lld", val, this->clock.master_cycles);

            this->via.regs.SR = val;
            return;
        case vACR: // aux control reg
            // TODO: more
            this->via.regs.ACR = val;
            return;
        case vPCR: // peripheral control reg
            // TODO: more
             // bit 0 - vblank IRQ control
             // bit 1-3 - one-second interrupt control
            this->via.regs.PCR = val;
            return;
        case vBufA: {// Write Data Reg A

            this->via.regs.IFR &= 0b11111100;
            mac_via_irq_sample(this);

            this->via.regs.ORA = val;
            update_via_RA(this);
            if ((this->via.regs.ORA & 0x20) != (val & 0x20)) {
                this->iwm.lines.SELECT = (val >> 5) & 1;
                printf("\nFLOPPY HEADSEL line via Via A to: %d", (val >> 5) & 1);
                mac_iwm_control(this, addr, 0, 0);;
            }


            printf("\nwrite VIA BufA data:%02x cyc:%lld", val, this->clock.master_cycles);
            return;}
        case vBufB: {// write Data Reg B
            this->via.regs.IFR &= 0b11100111;
            mac_via_irq_sample(this);

            this->via.regs.ORB = val;

            update_via_RB(this);

            return;}
    }
    printf("\nUnhandled VIA write addr:%06x val:%04x cyc:%lld", addr, (val & mask) >> 8, this->clock.master_cycles);
}
u16 mac_mainbus_read_iwm(struct mac* this, u32 addr, u16 mask, u16 old, u32 has_effect)
{
    uint16_t result = mac_iwm_read(this, (addr >> 9) & 15);
    return (result << 8) | result;
}

void mac_mainbus_write_iwm(struct mac* this, u32 addr, u16 mask, u16 val)
{
    if (mask & 0xFF)
        mac_iwm_write(this, (addr >> 9) & 15, val & 0xFF);
    else
        mac_iwm_write(this, (addr >> 9) & 15, val >> 8);
}

u16 mac_mainbus_read_scc(struct mac* this, u32 addr, u16 mask, u16 old, u32 has_effect)
{
    /*if ((addr >= sccWBase) && (addr < (sccWBase + 10))) {
        printf("\nRead to SCC write addr:%06x cyc:%lld", addr, this->clock.master_cycles);
        return old;
    }*/
    u32 oaddr = addr;
    if (addr >= sccWBase) addr -= sccWBase;
    else if (addr >= sccRBase) addr -= sccRBase;

    switch(addr) {
        case scc_aData:
            printf("\nSCC aData read, unsupported, cyc:%lld", this->clock.master_cycles);
            return this->scc.regs.aData << 8;
        case scc_aCtl:
            printf("\nSCC aCtl read, unsupported, cyc:%lld", this->clock.master_cycles);
            return this->scc.regs.aCtl << 8;
        case scc_bData:
            printf("\nSCC bData read, unsupported, cyc:%lld", this->clock.master_cycles);
            return this->scc.regs.bData << 8;
        case scc_bCtl:
            printf("\nSCC bCtl read, unsupported, cyc:%lld", this->clock.master_cycles);
            return this->scc.regs.bCtl << 8;
    }


    printf("\nUnhandled SCC read addr:%06x cyc:%lld", oaddr, this->clock.master_cycles);
    return 0xFFFF;
}

void mac_mainbus_write_scc(struct mac* this, u32 addr, u16 mask, u16 val)
{
    val >>= 8;
    /*if ((addr >= sccRBase) && (addr < (sccRBase + 10))) {
        printf("\nWrite to SCC read addr:%06x val:%02x cyc:%lld", addr, val, this->clock.master_cycles);
       return;
    }*/
    u32 oaddr = addr;
    if (addr >= sccWBase) addr -= sccWBase;
    else if (addr >= sccRBase) addr -= sccRBase;

    switch(addr) {
        case scc_aData:
            printf("\nSCC aData write, unsupported, val:%02x cyc:%lld", val, this->clock.master_cycles);
            this->scc.regs.aData = val;
            return;
        case scc_aCtl:
            printf("\nSCC aCtl write, unsupported, val:%02x cyc:%lld", val, this->clock.master_cycles);
            this->scc.regs.aCtl = val;
            return;
        case scc_bData:
            printf("\nSCC bData write, unsupported, val:%02x cyc:%lld", val, this->clock.master_cycles);
            this->scc.regs.bData = val;
            return;
        case scc_bCtl:
            printf("\nSCC bCtl write, unsupported, val:%02x cyc:%lld", val, this->clock.master_cycles);
            this->scc.regs.bCtl = val;
            return;
    }


    printf("\nUnhandled SCC write addr:%06x val:%04x cyc:%lld", oaddr, val & mask, this->clock.master_cycles);
}

static inline void write_RAM(struct mac* this, u32 addr, u16 mask, u16 val)
{
    this->RAM[(addr >> 1) & this->RAM_mask] = (this->RAM[(addr >> 1) & this->RAM_mask] & ~mask) | (val & mask);
}

static inline u16 read_ROM(struct mac* this, u32 addr, u16 mask, u16 old)
{
    return this->ROM[(addr >> 1) & this->ROM_mask] & mask;
}

static inline u16 read_RAM(struct mac* this, u32 addr, u16 mask, u16 old)
{
    return this->RAM[(addr >> 1) & this->RAM_mask];
}


void mac_mainbus_write(struct mac* this, u32 addr, u32 UDS, u32 LDS, u16 val)
{
    u16 mask = ulmask[(UDS << 1) | LDS];
    if (this->io.ROM_overlay) {
        if (addr < 0x100000) { // ROM
            return;
        }
        if ((addr >= 0x200000) && (addr < 0x300000)) { // ROM
            return;

        }
        if ((addr >= 0x400000) && (addr < 0x500000)) { // ROM
            return;
        }

        if ((addr >= 0x600000) && (addr < 0x800000)) {
            return write_RAM(this, addr - 0x600000, mask, val);
        }
    }
    else {

        if (addr < 0x400000) {
            return write_RAM(this, addr, mask, val);
        }
        if (addr < 0x500000) { // ROM
            return;
        }
    }


    if ((addr >= 0x900000) && (addr <= 0x9FFFFF)) {
        return mac_mainbus_write_scc(this, addr, mask, val);
    }

    if ((addr >= 0xB00000) && (addr <= 0xBFFFFF)) {
        return mac_mainbus_write_scc(this, addr, mask, val);
    }


    if ((addr >= 0xC00000) && (addr < 0xE00000)) {
        return mac_mainbus_write_iwm(this, addr, mask, val);
    }

    if ((addr >= 0xE80000) && (addr < 0xF00000)) {
        return mac_mainbus_write_via(this, addr, mask, val);
    }

    if ((addr >= 0xF80000) && (addr < 0xF8FFFF))
        return;

    printf("\nUnhandled mainbus write addr:%06x val:%04x sz:%d cyc:%lld", addr, val, UDS && LDS ? 2 : 1, this->clock.master_cycles);
}

u16 mac_mainbus_read(struct mac* this, u32 addr, u32 UDS, u32 LDS, u16 old, u32 has_effect)
{
    u16 mask = ulmask[(UDS << 1) | LDS];
    // 0-10k, 20-30k = ROM
    // 40-50k = ROM
    if (this->io.ROM_overlay) {
        if (addr < 0x100000)
            return read_ROM(this, addr, mask, old);
        if ((addr >= 0x200000) && (addr < 0x300000))
            return read_ROM(this, addr - 0x200000, mask, old);
        if ((addr >= 0x400000) && (addr < 0x500000))
            return read_ROM(this, addr, mask, old);
        if ((addr >= 0x600000) && (addr < 0x800000))
            return read_RAM(this, addr - 0x600000, mask, old);
    }
    else {
        if (addr < 0x400000)
            return read_RAM(this, addr, mask, old);

        if (addr < 0x500000)
            return read_ROM(this, addr - 0x400000, mask, old);
    }

    if ((addr >= 0x900000) && (addr <= 0x9FFFFF)) {
        //if (mask == 0xFFFF) return 0xFFFF;
        return mac_mainbus_read_scc(this, addr, mask, old, has_effect);
    }

    if ((addr >= 0xBF0000) && (addr <= 0xBFFFFF)) {
        return mac_mainbus_read_scc(this, addr, mask, old, has_effect);
    }

    if ((addr >= 0xC00000) && (addr < 0xE00000))
        return mac_mainbus_read_iwm(this, addr, mask, old, has_effect);

    if ((addr >= 0xE80000) && (addr < 0xF00000))
        return mac_mainbus_read_via(this, addr, mask, old, has_effect);

    if ((addr >= 0xF80000) && (addr < 0xF8FFFF)) // phase read
        return 0xFFFF;

    printf("\nUnhandled mainbus read addr:%06x cyc:%lld", addr, this->clock.master_cycles);
    return 0xFFFF;
}

