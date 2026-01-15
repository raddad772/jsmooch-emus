#include <cassert>
#include <cstdio>

#include "component/cpu/sm83/sm83.h"
#include "gb_bus.h"
#include "gb_clock.h"
#include "gb_ppu.h"
#include "gb.h"
#include "gb_debugger.h"

namespace GB {

#define GB_INSTANT_OAM false

static void raise_TIMA(CPU *th) {
    th->cpu.regs.IF |= 4;
}

timer::timer(void (*raise_IRQ_func)(CPU *), CPU *cpu_in) : raise_IRQ(raise_IRQ_func), raise_IRQ_ptr(cpu_in) {

}

// Increment 1 state
void timer::inc() {
    TIMA_reload_cycle = false;
    if (cycles_til_TIMA_IRQ > 0) {
        cycles_til_TIMA_IRQ--;
        if (cycles_til_TIMA_IRQ == 0) {
            DBG_EVENT(DBG_GB_EVENT_TIMER_IRQ);
            raise_IRQ(raise_IRQ_ptr);
            TIMA = TMA;
            TIMA_reload_cycle = true;
        }
    }
    SYSCLK_change((SYSCLK + 1) & 0xFFFF);
}

void timer::detect_edge(u32 before, u32 after)
{
    if ((before == 1) && (after == 0)) {
        TIMA = (TIMA + 1) & 0xFF; // Increment TIMA
        //DBG_EVENT(DBG_GB_EVENT_TIMER_TICK);
        if (TIMA == 0) { // If we overflow, schedule IRQ
            cycles_til_TIMA_IRQ = 1;
        }
    }
}

void timer::SYSCLK_change(u32 new_value) {
    // 00 = bit 9, lowest speed, /1024  4096 hz   & 0x200
    // 01 = bit 3,               /16  262144 hz   & 0x08
    // 10 = bit 5,               /64  65536 hz    & 0x20
    // 11 = bit 7,              /256  16384 hz    & 0x80
    SYSCLK = new_value;
    u32 this_bit = 0;
    switch (TAC & 3) {
    case 0: // using bit 7
        this_bit = (SYSCLK >> 7) & 1;
        break;
    case 3: // using bit 5
        this_bit = (SYSCLK >> 5) & 1;
        break;
    case 2: // using bit 3
        this_bit = (SYSCLK >> 3) & 1;
        break;
    case 1: // using bit 1
        this_bit = (SYSCLK >> 1) & 1;
        break;
        default:
            NOGOHERE;
    }
    this_bit &= ((TAC & 4) >> 2);

    // Detect falling edge...
    detect_edge(last_bit, this_bit);
    last_bit = this_bit;
}

void timer::write_IO(u32 addr, u32 val) {
    switch (addr) {
    case 0xFF04: // DIV, kind is upper 8 bits of SYSCLK
        SYSCLK_change(0);
        break;
    case 0xFF05: // TIMA, the timer counter
        if (!TIMA_reload_cycle) TIMA = val;
        // "During the strange cycle [A] you can prevent the IF flag from being set and prevent the TIMA from being reloaded from TMA by writing a value to TIMA. That new value will be the one that stays in the TIMA register after the instruction. Writing to DIV, TAC or other registers wont prevent the IF flag from being set or TIMA from being reloaded."
        if (cycles_til_TIMA_IRQ == 1) cycles_til_TIMA_IRQ = 0;
        break;
    case 0xFF06: // TMA, the timer modulo
        // "If TMA is written the same cycle it is loaded to TIMA [B], TIMA is also loaded with that value."
        if (TIMA_reload_cycle) TIMA = val;
        TMA = val;
        break;
    case 0xFF07: {
        // TAC, the timer control
        u32 old_bit = last_bit;
        last_bit &= ((val & 4) >> 2);

        detect_edge(old_bit, last_bit);
        TAC = val;
        break; }
    }
}

u32 timer::read_IO(u32 addr) const {
    switch (addr) {
        case 0xFF04: // DIV, upper 8 bits of SYSCLK
            return (SYSCLK >> 6) & 0xFF;
        case 0xFF05:
            return TIMA;
        case 0xFF06:
            return TMA;
        case 0xFF07:
            return TAC;
    }
    return 0xFF;
}

CPU::CPU(core* parent, GB::clock *clock_in, variants variant_in) :
    bus(parent),
    clock(clock_in),
    variant(variant_in),
    timer(&raise_TIMA, this) {
    for (unsigned char & FFreg : FFregs) {
        FFreg = 0;
    }
}

void CPU::reset() {
    cpu.reset();
    clock->cpu_frame_cycle = 0;
    clock->bootROM_enabled = true;
    dma.running = 0;
}

u32 GB_CPU_read_trace(void *tr, u32 addr)
{
    auto *th = static_cast<core *>(tr);
    return th->CPU_read(addr, 0, 0);
}

void CPU::enable_tracing() {
    if (tracing) return;
    jsm_debug_read_trace a;
    a.ptr = static_cast<void *>(this);
    a.read_trace = &GB_CPU_read_trace;
    cpu.enable_tracing(&a);
    tracing = true;
}

void CPU::disable_tracing() {
    if (!tracing) return;
    cpu.disable_tracing();
    tracing = false;
}

// Color GameBoy DMA
void CPU::dma_eval() {
    if (dma.cycles_til) {
        dma.cycles_til--;
        if (dma.cycles_til == 0) {
            DBG_EVENT(DBG_GB_EVENT_OAM_DMA_START);
            dma.running = 1;
            dma.index = 0;
            dma.high = dma.new_high;
            clock->old_OAM_can = clock->CPU_can_OAM;
            clock->CPU_can_OAM = false;
        }
        else
            return;
    }
    if (!dma.running) return;
    if (dma.index >= 160) {
        dma.running = 0;
        clock->CPU_can_OAM = clock->old_OAM_can;
        return;
    }
    bus->ppu.write_OAM(0xFE00 | dma.index, bus->CPU_read(dma.high | dma.index, 0, 1));
    dma.index++;
}

u32 CPU::hdma_run() {
    // If we're enabled and in the right lines
    if (((clock->ppu_mode == PPU::HBLANK) && (clock->ly < 144)) || (!bus->ppu.enabled)) {
        // If we're in the middle of a 16-byte block, or we have been notified of HBLANK
        if (((hdma.dest_index & 0x0F) != 0) || (hdma.notify_hblank)) {
            hdma.til_next_byte--;
            if (hdma.til_next_byte < 1) {
                hdma.notify_hblank = false;
                u32 r = bus->CPU_read(hdma.source_index, 0xFF, 1);
                //console.log('RUN HDMA!', hex4(hdma.dest_index), hex4(hdma.source_index), hex2(r), clock->trace_cycles);
                bus->CPU_write(hdma.dest_index, r);
                hdma.dest_index = ((hdma.dest_index + 1) & 0x1FFF) | 0x8000;
                hdma.source_index = (hdma.source_index + 1) & 0xFFFF;
                // A 16-byte block has finished
                if ((hdma.dest_index & 0x0F) == 0) {
                    hdma.length--; // = (hdma.length - 1) & 0xFF;
                    if (hdma.length <= 0) { // Terminate HDMA
                        hdma.enabled = false;
                        hdma.waiting = false;
                        hdma.completed = true;
                        hdma.active = false;
                    }
                    else {
                        hdma.completed = false;
                        hdma.enabled = true;
                        hdma.waiting = true;
                    }
                }
                hdma.til_next_byte = clock->turbo ? 2 : 1; // If we're at turbo-speed, we need to wait 2 M-cycles in between transfer. If not, 1.
            }
            return true; // we RAN so we RETURN true
        }
    }
    return false;
}

void CPU::ghdma_run()
{
    hdma.til_next_byte--;
    if (hdma.til_next_byte < 1) {
        // Copy byte
        bus->CPU_write(hdma.dest_index, bus->CPU_read(hdma.source_index, 0xFF, 1));
        hdma.dest_index = ((hdma.dest_index + 1) & 0x1FFF) | 0x8000;
        hdma.source_index = (hdma.source_index + 1) & 0xFFFF;
        // A 16-byte block has finished
        if ((hdma.dest_index & 0x0F) == 0) {
            hdma.length = (hdma.length - 1) & 0xFF;
            if (hdma.length == 0xFF) { // Terminate HDMA
                hdma.enabled = false;
                hdma.waiting = false;
                hdma.completed = true;
                hdma.active = false;
            }
        }
        hdma.til_next_byte = clock->turbo ? 2 : 1; // If we're at turbo-speed, we need to wait 2 M-cycles in between transfer. If not, 1.
    }
}

u32 CPU::hdma_eval()
{
    if ((clock->cgb_enable) && (hdma.enabled)) {
        if (hdma.mode == 0) {
            ghdma_run();
            return true;
        } else {
            return hdma_run();
        }
    }
    return false;
}

void CPU::switch_speed() const
{
    if (!clock->cgb_enable) return;
    if (clock->timing.cpu_divisor == 4) {
        clock->timing.cpu_divisor = 2;
        clock->turbo = true;
    }
    else {
        clock->timing.cpu_divisor = 4;
        clock->turbo = false;
    }
}

void CPU::cycle()
{
    clock->trace_cycles++;

    if (hdma_eval()) {
        timer.inc();
        return;
    }

    if ((cpu.regs.STP) && (io.speed_switch_prepare)) {
        if (io.speed_switch_cnt < 0) {
            io.speed_switch_cnt = static_cast<i32>(0x10000 - clock->SYSCLK); // Speed switchover actually happens at rollover of SYSCLK
        }
        io.speed_switch_cnt--;
        if (io.speed_switch_cnt == 0) {
            switch_speed();
            io.speed_switch_prepare = 0;
            cpu.regs.TCU++;
            cpu.regs.STP = 0;
            clock->SYSCLK = 0;
        }
    }

    // Service CPU reads and writes
    if (cpu.pins.MRQ) {
        if (cpu.pins.RD) {
            cpu.pins.D = bus->CPU_read(cpu.pins.Addr, 0xCC, 1);
            if (tracing) {
                // TODO: debug
                //dbg.traces.add(TRACERS.SM83, cpu.trace_cycles, trace_format_read('SM83', SM83_COLOR, cpu.trace_cycles, cpu.pins.Addr, cpu.pins.D));
            }
        }
        if (cpu.pins.WR) {
            if ((!dma.running) || (cpu.pins.Addr >= 0xFF00))
                bus->CPU_write(cpu.pins.Addr, cpu.pins.D);

            if (tracing) {
                // TODO: debug
                //dbg.traces.add(TRACERS.SM83, cpu.trace_cycles, trace_format_write('SM83', SM83_COLOR, cpu.trace_cycles, cpu.pins.Addr, cpu.pins.D));
            }
        }
    }
    cpu.cycle();
    dma_eval();
    if (cpu.regs.STP)
        timer.SYSCLK_change(0);
    else {
        timer.inc();
    }
    clock->SYSCLK = timer.SYSCLK;
}

void CPU::quick_boot()
{
    switch (variant) {
    case DMG:
        cpu.regs.A = 1;
        cpu.regs.F.Z = 0;
        cpu.regs.SP = 0xFFFE;
        cpu.regs.B = 0;
        cpu.regs.C = 0x13;
        cpu.regs.D = 0;
        cpu.regs.E = 0xD8;
        cpu.regs.H = 0x01;
        cpu.regs.L = 0x4D;
        cpu.regs.PC = 0x101;
        cpu.regs.TCU = 0;
        cpu.regs.IR = SM83::INS_DECODE;
        cpu.regs.IME = 0;
        cpu.regs.IE = 0;
        cpu.regs.IF = 1;
        hdma.enabled = false;
        timer.TIMA = 0;
        timer.TMA = 0;
        timer.TAC = 0xF8;
        clock->bootROM_enabled = false;
        cpu.pins.Addr = 0x100;
        cpu.pins.MRQ = cpu.pins.RD = true;
        cpu.pins.WR = false;
        cpu.regs.poll_IRQ = true;
        break;
    case GBC:
        cpu.regs.A = 0x11;
        cpu.regs.F.Z = 1;
        //cpu.regs.F.N = cpu.regs.F.H = cpu.regs.F.C = 0;
        cpu.regs.SP = 0xFFFE;
        cpu.regs.B = 0;
        cpu.regs.C = 0;
        cpu.regs.D = 0xFF;
        cpu.regs.E = 0x56;
        cpu.regs.H = 0;
        cpu.regs.L = 0x0D;
        cpu.regs.PC = 0x101;
        cpu.regs.TCU = 0;
        cpu.regs.IR = SM83::INS_DECODE;
        cpu.regs.IME = 0;
        cpu.regs.IE = 0;
        cpu.regs.IF = 1;
        timer.TIMA = 0;
        timer.TMA = 0;
        timer.TAC = 0xF8;
        clock->bootROM_enabled = false;
        hdma.dest = 0xFFFF;
        hdma.source = 0xFFFF;
        hdma.length = 0x7F;
        hdma.mode = 1;
        hdma.enabled = false;
        // TODO: mapper RESET
        /*
        bus.mapper.VRAM_bank_offset = 8192; // 0xFF startup register value
        bus.mapper.WRAM_bank_offset = 4096 * 7; // 0xFF startup register value
         */
        cpu.pins.Addr = 0x100;
        cpu.pins.MRQ = cpu.pins.RD = true;
        cpu.pins.WR = false;
        cpu.regs.poll_IRQ = true;
        break;
    default:
        printf("FAST BOOT NOT ENABLED FOR THIS VARIANT!");
        break;
    }
}

void CPU::update_inputs() {
    auto &blst = device_ptr.get().controller.digital_buttons;
    HID_digital_button* b;
#define B_GET(button, num) { b = &blst.at(num); input_buffer. button = b->state; }
    B_GET(up, 0);
    B_GET(down, 1);
    B_GET(left, 2);
    B_GET(right, 3);
    B_GET(a, 4);
    B_GET(b, 5);
    B_GET(start, 6);
    B_GET(select, 7);
#undef B_GET
}

u32 CPU::get_input() {
    u32 out1;
    u32 out3 = 0x0F;
    update_inputs();
    if (io.JOYP.action_select == 0) {
        out1 = input_buffer.a | (input_buffer.b << 1) | (input_buffer.select << 2) | (input_buffer.start << 3);
        out1 ^= 0x0F;
        out3 &= out1;
    }

    if (io.JOYP.direction_select == 0) {
        out1 = input_buffer.right | (input_buffer.left << 1) | (input_buffer.up << 2) | (input_buffer.down << 3);
        out1 ^= 0x0F;
        out3 &= out1;
    }
    return out3;
}

u32 CPU::read_IO(u32 addr, u32 val)
{
    if ((addr >= 0xFF10) && (addr < 0xFF40)) {
        return bus->apu.read_IO(addr, val, true);
    }
    switch(addr) {
        case 0xFF00: // JOYP
            // return not pressed=1 in bottom 4 bits
            return get_input() | (io.JOYP.action_select << 4) | (io.JOYP.direction_select << 5) | 0xC0;
        case 0xFF01: // SB serial
            //return FFregs[1];
            return 0xFF;
        case 0xFF02: // SC
            return val;
        case 0xFF04: // DIV
        case 0xFF05: // TIMA
        case 0xFF06: // TIMA reload
        case 0xFF07: // TAC timer control
            return timer.read_IO(addr);
        case 0xFF0F: // IF: interrupt flag
            //return cpu.regs.IF & 0x1F;
            return cpu.regs.IF | 0xE0;
            //return clock->irq.vblank_request | (clock->irq.lcd_stat_request << 1) | (clock->irq.timer_request << 2) | (clock->irq.serial_request << 3) | (clock->irq.joypad_request << 4);
        case 0xFF46: // OAM DMA
            return dma.last_write;
        case 0xFF4D: // Speed switch enable
            if (!clock->cgb_enable) return 0xFF;
            return io.speed_switch_prepare | (clock->turbo ? 0x80 : 0);
        case 0xFF4F: // VRAM bank
            if (!clock->cgb_enable) return 0xFF;
            return bus->VRAM_bank;
        case 0xFF51: // HDMA1 MSB
            if (!clock->cgb_enable) return 0xFF;
            return (hdma.source & 0xFF00) >> 8;
        case 0xFF52: // HDMA2 LSB
            if (!clock->cgb_enable) return 0xFF;
            return hdma.source & 0xFF;
        case 0xFF53: // HDMA3 MSB
            if (!clock->cgb_enable) return 0xFF;
            return (hdma.dest & 0xFF00) >> 8;
        case 0xFF54: // HDMA4 LSB
            if (!clock->cgb_enable) return 0xFF;
            return hdma.dest & 0xFF;
        case 0xFF55: // HDMA5
            if (!clock->cgb_enable) return 0xFF;
            return (hdma.enabled ? 0 : 0x80) | hdma.length;
        case 0xFF70: // WRAM bank
            if (!clock->cgb_enable) return 0xFF;
            return bus->WRAM_bank;
        case 0xFFFF: // IE Interrupt Enable
            //return cpu.regs.IE & 0x1F;
            return cpu.regs.IE | 0xE0;
            //return clock->irq.vblank_enable | (clock->irq.lcd_stat_enable << 1) | (clock->irq.timer_enable << 2) | (clock->irq.serial_enable << 3) | (clock->irq.joypad_enable << 4);
    }
    //if (addr == 0xFF44) printf("\nHEY VAL GOES OUT %02x", val);
    return val;
    // TODO: reenable APU reads here?
    //return bus->APU_read_IO(addr, val);
}

void CPU::write_IO(u32 addr, u32 val)
{
    if ((addr >= 0xFF10) && (addr < 0xFF40)) {
        bus->apu.write_IO(addr, val);
        return;
    }
    switch(addr) {
        case 0xFF00: // JOYP
            io.JOYP.action_select = (val & 0x20) >> 5;
            io.JOYP.direction_select = (val & 0x10) >> 4;
            return;
        case 0xFF01: // SB serial
            FFregs[1] = val;
            return;
        case 0xFF02: // SC
            FFregs[2] = val;
            //cycles_til_serial_interrupt =
            return;
        case 0xFF04: // DIV
        case 0xFF05: // TIMA
        case 0xFF06: // TIMA reload
        case 0xFF07: // TAC TIMA controler
            timer.write_IO(addr, val);
            return;
        case 0xFF46: // OAM DMA
            if (GB_INSTANT_OAM) {
                dma.high = (val << 8);
                for (u32 i = 0; i < 160; i++) {
                    bus->ppu.write_OAM(0xFE00 | i, bus->CPU_read(dma.high | i, 0, 1));
                }
            } else {
                dma.cycles_til = 2;
                ////dma.running = 1;
                dma.new_high = (val << 8);
                dma.last_write = val;
            }
            return;
        case 0xFF4D: // Speed switch enable
            if (!clock->cgb_enable) return;
            //console.log('PREPARE SPEED SWITCH');
            io.speed_switch_prepare = val & 1;
            return;
        case 0xFF4F: // VRAM bank
            if (!clock->cgb_enable) return;
            bus->VRAM_bank = val;
            bus->generic_mapper.VRAM_bank_offset = 8192 * (val & 1);
            return;
        case 0xFF50: // Boot ROM disable. Cannot re-enable
            if (val & 1) {
                printf("Disable boot ROM!");
                //dbg.break();
                clock->bootROM_enabled = false;
            }
            return;
        case 0xFF51: // HDMA1   bits 15-8 of dest
            if (!clock->cgb_enable) return;
            //hdma.source = (hdma.source & 0xF0) | (val << 8);
            hdma.source = (hdma.source & 0xFF) | (val << 8);
            return;
        case 0xFF52: // HDMA2   bits 7-4 of dest
            if (!clock->cgb_enable) return;
            hdma.source = (hdma.source & 0xFF00) | val;
            //hdma.source = (hdma.source & 0xFF00) | (val & 0xF0);
            return;
        case 0xFF53: // HDMA3  bits 12-8 of dest
            if (!clock->cgb_enable) return;
            hdma.dest = (hdma.dest & 0xFF) | (val << 8);
            //hdma.dest = 0x8000 | (hdma.dest & 0x1F00) | (val & 0xF0);
            return;
        case 0xFF54: // HDMA4 bits
            if (!clock->cgb_enable) return;
            hdma.dest = (hdma.dest & 0xFF00) | val;
            //hdma.dest = 0x8000 | (hdma.dest & 0xF0) | ((val & 0x1F) << 8);
            return;
        case 0xFF55: // HDMA5 transfer stuff!
            if (!clock->cgb_enable) return;
            //console.log('HDMA5 TRANSFER START', clock->trace_cycles, hex2(val));
            // if LCD ON...
            if (bus->ppu.enabled) {
                hdma.notify_hblank = (clock->ppu_mode == 0);
            } else {// if LCD OFF... {
                hdma.notify_hblank = true;
                //console.log('LCD OFF SO NOTIFY HBLANK!');
            }
            hdma.mode = (val & 0x80) >> 7;
            hdma.length = (val & 0x7F) + 1; // up to 128 blocks of 16 bytes.
            if (!hdma.enabled) {
                printf("\nHDMA TRANSFER");
                hdma.enabled = true;
                if (hdma.mode == 0) hdma.active = true;
            } else {
                if ((val & 0x80) == 0) {
                    //console.log('HDMA TRANSFER CANCEL!');
                    hdma.enabled = false;
                    hdma.active = false;
                }
            }
            hdma.dest_index = (hdma.dest & 0x1FF0) | 0x8000;
            hdma.source_index = hdma.source & 0xFFF0;
            //hdma.last_ly = 250;

            hdma.til_next_byte = clock->turbo ? 2 : 1; // If we're at turbo-speed, we need to wait 2 M-cycles in between transfer. If not, 1.
            return;
        case 0xFF6C:
            if (!clock->cgb_enable) return;
            printf("OBJPRIOR WRITE! %d", val);
            return;
        case 0xFF70: // WRAM bank
            if (!clock->cgb_enable) return;
            bus->WRAM_bank = val;
            val &= 7;
            if (val == 0) val = 1;
            bus->generic_mapper.WRAM_bank_offset = 4096 * val;
            return;
        case 0xFF0F:
            //console.log('WRITE IF', val & 0x1F);
            cpu.regs.IF = val & 0x1F;
            return;
        case 0xFFFF: // IE: Interrupt Enable
            cpu.regs.IE = val & 0x1F;
            return;
    }
    //TODO: APU stuff
    //bus->apu.write_IO(addr, val);
}
}