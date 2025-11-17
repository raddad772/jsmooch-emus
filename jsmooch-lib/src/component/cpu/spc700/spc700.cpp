//
// Created by . on 4/16/25.
//
#include "spc700.h"
#include "spc700_boot_rom.h"
#include "spc700_disassembler.h"

extern SPC700_ins_func spc700_decoded_opcodes[0x100];

void SPC700::reset()
{
    io.ROM_readable = 1;
    for (auto & timer : timers) {
        timer.out = 0xF;
    }

    for (unsigned char & i : RAM) {
        i = 0;
    }

    regs.PC = SPC700_boot_rom[62] + (SPC700_boot_rom[63] << 8);

    regs.SP = 0xEF;
    regs.P.v = 2;
}


static SPC700_ins_func get_decoded_opcode(u32 opcode)
{
    return spc700_decoded_opcodes[opcode];
}

void SPC700::advance_timers(i32 num_cycles)
{
    timers[0].divider += num_cycles;
    timers[1].divider += num_cycles;
    timers[2].divider += num_cycles;

    if (!io.T0_enable)
        timers[0].divider &= 127;
    else {
        while(timers[0].divider >= 128) {
            timers[0].counter = (timers[0].counter + 1) & 255;
            if (timers[0].counter == timers[0].target) {
                timers[0].counter = 0;
                timers[0].out = (timers[0].out + 1) & 15;
            }
            timers[0].divider -= 128;
        }
    }

    if (!io.T1_enable)
        timers[1].divider &= 127;
    else {
        while(timers[1].divider >= 128) {
            timers[1].counter = (timers[1].counter + 1) & 255;
            if (timers[1].counter == timers[1].target) {
                timers[1].counter = 0;
                timers[1].out = (timers[1].out + 1) & 15;
            }
            timers[1].divider -= 128;
        }
    }

    if (!io.T2_enable)
        timers[2].divider &= 15;
    else {
        while(timers[2].divider >= 16) {
            timers[2].counter = (timers[2].counter + 1) & 255;
            if (timers[2].counter == timers[2].target) {
                timers[2].counter = 0;
                timers[2].out = (timers[2].out + 1) & 15;
            }
            timers[2].divider -= 16;
        }
    }
}

void SPC700::trace_format()
{
    //dbg.traces.add(TRACERS.SPC, this.clock.apu_has, this.trace_format(.disassemble(), (.regs.PC - 1) & 0xFFFF));
    u32 do_dbglog = 0;
    if (trace.dbglog.view && trace.dbglog.view->ids_enabled[trace.dbglog.id]) {
        trace.str.quickempty();
        trace.str2.quickempty();
        // PC, read, out, p_p
        SPC700_disassemble(regs.PC, trace.strct, trace.str, regs.P.P);
        trace.str2.sprintf("A:%02x  X:%02x  Y:%02x  SP:%04x  P:%c%c%c%c%c%c%c%c",
                           regs.A,
                           regs.X, regs.Y,
                           regs.PC,
                           regs.P.C ? 'C' : 'c',
                           regs.P.Z ? 'Z' : 'z',
                           regs.P.I ? 'I' : 'i',
                           regs.P.H ? 'H' : 'h',
                           regs.P.B ? 'B' : 'b',
                           regs.P.P ? 'P' : 'p',
                           regs.P.V ? 'V' : 'v',
                           regs.P.N ? 'N' : 'n'
                           );

        u64 tc;
        if (!clock) tc = 0;
        else tc = *clock;

        dbglog_view *dv = trace.dbglog.view;
        dv->add_printf(trace.dbglog.id, tc, DBGLS_TRACE, "%04x    %s", trace.ins_PC, trace.str.ptr);
        dv->extra_printf("%s", trace.str2.ptr);
    }
}

void SPC700::cycle(i64 how_many) {
    cycles += how_many;
    while(cycles > 0) {
        regs.opc_cycles = 0;
        if (STP || WAI) {
            static int a = 1;
            if (a) {
                a = 0;
                printf("\nWARN STP OR WAI ON SPC700");
                dbg_break("SPC700 STP OR BRK", *clock);
            }
            return;
        }

        regs.IR = read8(regs.PC);
        trace.ins_PC = regs.PC;
        trace_format();

        regs.PC = (regs.PC + 1) & 0xFFFF;
        SPC700_ins_func fptr = get_decoded_opcode(regs.IR);
        fptr(this);
        (*clock) += regs.opc_cycles;
        cycles -= regs.opc_cycles;
        int_clock += regs.opc_cycles;
        advance_timers(regs.opc_cycles);
    }
}

u8 SPC700::readIO(u32 addr)
{
    u32 val;
    switch(addr) {
        case 0xF0: // TEST register we do not emulate
            return 0x0A;
        case 0xF1: // CONTROL - I/O and timer control
            val = io.ROM_readable << 7;
            val += 0x30;
            val += io.T2_enable << 2;
            val += io.T1_enable << 1;
            val += io.T0_enable;
            return val;
        case 0xF2:
            return io.DSPADDR;
        case 0xF3:
            return read_dsp(read_ptr, io.DSPADDR);
        case 0xF4:
            return io.CPUI[0];
        case 0xF5:
            return io.CPUI[1];
        case 0xF6:
            return io.CPUI[2];
        case 0xF7:
            return io.CPUI[3];
        case 0xF8:
        case 0xF9:
            return RAM[addr];
        case 0xFA: // Read-only
        case 0xFB:
        case 0xFC:
            return 0;
        case 0xFD:
            val = timers[0].out;
            timers[0].out = 0;
            return val;
        case 0xFE:
            val = timers[1].out;
            timers[1].out = 0;
            return val;
        case 0xFF:
            val = timers[2].out;
            timers[2].out = 0;
            return val;
        default:
            printf("\nUNEMULATED SPC REG %04x", addr);
            return 0;
    }
}

void SPC700::writeIO(u32 addr, u32 val)
{
    switch(addr) {
        case 0xF0: // TEST register, should not be written
            if (val != 0x0A) {
                printf("\nWARN SPC700 WRITE REG 0xF0 %02x", val);
            }
            return;
        case 0xF1: // CONTROL reg
            //io.ROM_readable = (val >> 7) & 1;
            if (val & 0x20) io.CPUI[2] = io.CPUI[3] = 0;
            if (val & 0x10) io.CPUI[0] = io.CPUI[1] = 0;
            io.T2_enable = (val >> 2) & 1;
            io.T1_enable = (val >> 1) & 1;
            io.T0_enable = val & 1;
            return;
        case 0xF2:
            io.DSPADDR = val;
            return;
        case 0xF3:
            write_dsp(write_ptr, io.DSPADDR, val);
            return;
        case 0xF4:
        case 0xF5:
        case 0xF6:
        case 0xF7:
            io.CPUO[addr - 0xF4] = val;
            //printf("\nAPU (%04X) to CPU (%d): %02X", addr & 0xFFFF, addr - 0xF4, val);
            return;
        case 0xF8:
        case 0xF9:
            RAM[addr] = val;
            return;
        case 0xFA:
            timers[0].target = val;
            return;
        case 0xFB:
            timers[1].target = val;
            return;
        case 0xFC:
            timers[2].target = val;
            return;
        case 0xFD:
        case 0xFE:
        case 0xFF:
            // Read-only
            return;
        default:
    }
}

void SPC700::log_write(u32 addr, u32 val)
{
    if (trace.dbglog.view && trace.dbglog.view->ids_enabled[trace.dbglog.id_write]) {
        u64 tc;
        if (!clock) tc = 0;
        else tc = *clock;

        dbglog_view *dv = trace.dbglog.view;
        /*if (addr == 1) {
            dbglog_view_add_printf(dv, trace.dbglog.id_write, tc, DBGLS_TRACE, "%04x     (write WriteAdr) %02x", addr, val);
            //printf("\nNEW WriteAdr: %04x  cyc:%lld", RAM[0] | (RAM[1] << 8), *clock);
        }
        else if ((addr >= 0xF4) && (addr <= 0xF7)) {
            // addr - 0xF4
            dbglog_view_add_printf(dv, trace.dbglog.id_write, tc, DBGLS_TRACE, "%04x     (write MAIL%d) %02x", addr, addr - 0xF4, val);
        }
        else if (addr == 0xF1) {
            dbglog_view_add_printf(dv, trace.dbglog.id_write, tc, DBGLS_TRACE, "%04x     (write CTRL REG) %02x", addr, val);
            //printf("\nWRITE CTRL REG %02x", val);

        }
        else {*/
            //dbglog_view_add_printf(dv, trace.dbglog.id_write, tc, DBGLS_TRACE, "%04x     (write) %02x", addr, val);
        //}
    }
}


void SPC700::log_read(u32 addr, u32 val)
{
    if (trace.dbglog.view && trace.dbglog.view->ids_enabled[trace.dbglog.id_read]) {
        u64 tc;
        if (!clock) tc = 0;
        else tc = *clock;

        dbglog_view *dv = trace.dbglog.view;

        /*if (addr == 0) {
            dbglog_view_add_printf(dv, trace.dbglog.id_read, tc, DBGLS_TRACE, "%04x     (READ WriteAdr) %02x", addr, val);
        }
        else if ((addr >= 0xF4) && (addr <= 0xF7)) {
            // addr - 0xF4
            dv->add_printf(trace.dbglog.id_read, tc, DBGLS_TRACE, "%04x     (read MAIL%d) %02x", addr, addr - 0xF4, val);
        }
        else {*/
            dv->add_printf(trace.dbglog.id_read, tc, DBGLS_TRACE, "%04x     (read) %02x", addr, val);
        //}
    }
}

u8 SPC700::read8(u32 addr)
{
    u8 r;
    if ((addr >= 0x00F1) && (addr <= 0x00FF)) r = readIO(addr);
    else if ((addr >= 0xFFC0) && io.ROM_readable) r = SPC700_boot_rom[addr - 0xFFC0];
    else r = RAM[addr & 0xFFFF];

    log_read(addr, r);

    return r;
}

u8 SPC700::read8D(u32 addr)
{
    return read8((addr & 0xFF) + regs.P.DO);
}

void SPC700::write8(u32 addr, u32 val)
{
    if ((addr >= 0x00F1) && (addr <= 0x00FF))
        writeIO(addr, val);
    RAM[addr & 0xFFFF] = val;
    log_write(addr, val);
}

void SPC700::write8D(u32 addr, u32 val)
{
    write8((addr & 0xFF) + regs.P.DO, val);
}

static u32 read_trace(void *ptr, u32 addr)
{
    auto *th = static_cast<SPC700 *>(ptr);
    u8 r;
    if ((addr >= 0x00F1) && (addr <= 0x00FF)) r = th->readIO(addr);
    else if ((addr >= 0xFFC0) && th->io.ROM_readable) r = SPC700_boot_rom[addr - 0xFFC0];
    else r = th->RAM[addr & 0xFFFF];

    return r;
}

void SPC700::setup_tracing(jsm_debug_read_trace *strct)
{
    trace.strct.ptr = this;
    trace.strct.read_trace = &read_trace;
    trace.ok = 1;
}

