#include <assert.h>
#include <stdio.h>

#include "helpers_c/debugger/debugger.h"

#include "gb_cpu.h"
#include "component/cpu/sm83/sm83.h"
#include "gb_bus.h"
#include "gb_clock.h"
#include "gb_ppu.h"
#include "gb.h"
#include "gb_debugger.h"

#define GB_INSTANT_OAM false

void GB_CPU_raise_TIMA(struct GB_CPU *this) {
    this->cpu.regs.IF |= 4;
}

void GB_timer_SYSCLK_change(struct GB_timer* this, u32 new_value);

void GB_timer_init(struct GB_timer* this, void (*raise_IRQ)(struct GB_CPU *), struct GB_CPU *cpu) {
    this->SYSCLK = 0;
    this->cycles_til_TIMA_IRQ = 0;
    this->raise_IRQ = raise_IRQ;
    this->raise_IRQ_cpu = cpu;

    DBG_EVENT_VIEW_INIT;

    this->TIMA = this->TMA = this->TAC = this->last_bit = this->SYSCLK = this->cycles_til_TIMA_IRQ = 0;
    this->TIMA_reload_cycle = FALSE;
}

// Increment 1 state
void GB_timer_inc(struct GB_timer* this) {
    this->TIMA_reload_cycle = FALSE;
    if (this->cycles_til_TIMA_IRQ > 0) {
        this->cycles_til_TIMA_IRQ--;
        if (this->cycles_til_TIMA_IRQ == 0) {
            DBG_EVENT(DBG_GB_EVENT_TIMER_IRQ);
            this->raise_IRQ(this->raise_IRQ_cpu);
            this->TIMA = this->TMA;
            this->TIMA_reload_cycle = TRUE;
        }
    }
    GB_timer_SYSCLK_change(this, (this->SYSCLK + 1) & 0xFFFF);
}

void GB_timer_detect_edge(struct GB_timer *this, u32 before, u32 after)
{
    if ((before == 1) && (after == 0)) {
        this->TIMA = (this->TIMA + 1) & 0xFF; // Increment TIMA
        //DBG_EVENT(DBG_GB_EVENT_TIMER_TICK);
        if (this->TIMA == 0) { // If we overflow, schedule IRQ
            this->cycles_til_TIMA_IRQ = 1;
        }
    }
}

void GB_timer_SYSCLK_change(struct GB_timer *this, u32 new_value) {
    // 00 = bit 9, lowest speed, /1024  4096 hz   & 0x200
    // 01 = bit 3,               /16  262144 hz   & 0x08
    // 10 = bit 5,               /64  65536 hz    & 0x20
    // 11 = bit 7,              /256  16384 hz    & 0x80
    this->SYSCLK = new_value;
    u32 this_bit = 0;
    switch (this->TAC & 3) {
    case 0: // using bit 7
        this_bit = (this->SYSCLK >> 7) & 1;
        break;
    case 3: // using bit 5
        this_bit = (this->SYSCLK >> 5) & 1;
        break;
    case 2: // using bit 3
        this_bit = (this->SYSCLK >> 3) & 1;
        break;
    case 1: // using bit 1
        this_bit = (this->SYSCLK >> 1) & 1;
        break;
    }
    this_bit &= ((this->TAC & 4) >> 2);

    // Detect falling edge...
    GB_timer_detect_edge(this, this->last_bit, this_bit);
    this->last_bit = this_bit;
}

void GB_timer_write_IO(struct GB_timer *this, u32 addr, u32 val) {
    u32 last_bit;
    switch (addr) {
    case 0xFF04: // DIV, kind is upper 8 bits of SYSCLK
        GB_timer_SYSCLK_change(this, 0);
        break;
    case 0xFF05: // TIMA, the timer counter
        if (!this->TIMA_reload_cycle) this->TIMA = val;
        // "During the strange cycle [A] you can prevent the IF flag from being set and prevent the TIMA from being reloaded from TMA by writing a value to TIMA. That new value will be the one that stays in the TIMA register after the instruction. Writing to DIV, TAC or other registers wont prevent the IF flag from being set or TIMA from being reloaded."
        if (this->cycles_til_TIMA_IRQ == 1) this->cycles_til_TIMA_IRQ = 0;
        break;
    case 0xFF06: // TMA, the timer modulo
        // "If TMA is written the same cycle it is loaded to TIMA [B], TIMA is also loaded with that value."
        if (this->TIMA_reload_cycle) this->TIMA = val;
        this->TMA = val;
        break;
    case 0xFF07: // TAC, the timer control
        last_bit = this->last_bit;
        this->last_bit &= ((val & 4) >> 2);

        GB_timer_detect_edge(this, last_bit, this->last_bit);
        this->TAC = val;
        break;
    }
}

u32 GB_timer_read_IO(struct GB_timer *this, u32 addr) {
    switch (addr) {
        case 0xFF04: // DIV, upper 8 bits of SYSCLK
            return (this->SYSCLK >> 6) & 0xFF;
        case 0xFF05:
            return this->TIMA;
        case 0xFF06:
            return this->TMA;
        case 0xFF07:
            return this->TAC;
    }
    return 0xFF;
}


void GB_CPU_init(struct GB_CPU* this, enum GB_variants variant, struct GB_clock* clock, struct GB_bus* bus)
{
    this->variant = variant;
    this->clock = clock;
    this->bus = bus;
    bus->cpu = this;

    this->input_buffer.a = this->input_buffer.b = 0;
    this->input_buffer.start = this->input_buffer.select = 0;
    this->input_buffer.up = this->input_buffer.down = 0;
    this->input_buffer.left = this->input_buffer.right = 0;
    DBG_EVENT_VIEW_INIT;

    this->tracing = 0;

    SM83_init(&this->cpu);
    for (u32 i = 0; i < 256; i++) {
        this->FFregs[i] = 0;
    }

    GB_timer_init(&this->timer, &GB_CPU_raise_TIMA, this);

    this->tracing = FALSE;

    this->io.JOYP.action_select = this->io.JOYP.direction_select = 0;
    this->io.speed_switch_prepare = 0;
    this->io.speed_switch_cnt = -1;
    this->dma.cycles_til = this->dma.high = this->dma.index = this->dma.last_write = this->dma.new_high = this->dma.running = 0;
}

void GB_CPU_reset(struct GB_CPU* this) {
    SM83_reset(&this->cpu);
    this->clock->cpu_frame_cycle = 0;
    this->clock->bootROM_enabled = TRUE;
    this->dma.running = 0;
}

u32 GB_CPU_read_trace(void *tr, u32 addr)
{
    return GB_bus_CPU_read(((struct GB_CPU *)tr)->bus, addr, 0, 0);
}

void GB_CPU_enable_tracing(struct GB_CPU* this) {
    if (this->tracing) return;
    struct jsm_debug_read_trace a;
    a.ptr = (void *)this;
    a.read_trace = &GB_CPU_read_trace;
    SM83_enable_tracing(&this->cpu, &a);
    this->tracing = TRUE;
}

void GB_CPU_disable_tracing(struct GB_CPU* this) {
    if (!this->tracing) return;
    SM83_disable_tracing(&this->cpu);
    this->tracing = FALSE;
}

// Color GameBoy DMA
void GB_CPU_dma_eval(struct GB_CPU* this) {
    if (this->dma.cycles_til) {
        this->dma.cycles_til--;
        if (this->dma.cycles_til == 0) {
            DBG_EVENT(DBG_GB_EVENT_OAM_DMA_START);
            this->dma.running = 1;
            this->dma.index = 0;
            this->dma.high = this->dma.new_high;
            this->clock->old_OAM_can = this->clock->CPU_can_OAM;
            this->clock->CPU_can_OAM = 0;
        }
        else
            return;
    }
    if (!this->dma.running) return;
    if (this->dma.index >= 160) {
        this->dma.running = 0;
        this->clock->CPU_can_OAM = this->clock->old_OAM_can;
        return;
    }
    this->bus->write_OAM(this->bus, 0xFE00 | this->dma.index, GB_bus_CPU_read(this->bus, this->dma.high | this->dma.index, 0, 1));
    this->dma.index++;
}

u32 GB_CPU_hdma_run(struct GB_CPU *this) {
    // If we're enabled and in the right lines
    if (((this->clock->ppu_mode == HBLANK) && (this->clock->ly < 144)) || (!this->bus->ppu->enabled)) {
        // If we're in the middle of a 16-byte block, or we have been notified of HBLANK
        if (((this->hdma.dest_index & 0x0F) != 0) || (this->hdma.notify_hblank)) {
            this->hdma.til_next_byte--;
            if (this->hdma.til_next_byte < 1) {
                this->hdma.notify_hblank = false;
                u32 r = GB_bus_CPU_read(this->bus, this->hdma.source_index, 0xFF, 1);
                //console.log('RUN HDMA!', hex4(this->hdma.dest_index), hex4(this->hdma.source_index), hex2(r), this->clock->trace_cycles);
                GB_bus_CPU_write(this->bus, this->hdma.dest_index, r);
                this->hdma.dest_index = ((this->hdma.dest_index + 1) & 0x1FFF) | 0x8000;
                this->hdma.source_index = (this->hdma.source_index + 1) & 0xFFFF;
                // A 16-byte block has finished
                if ((this->hdma.dest_index & 0x0F) == 0) {
                    this->hdma.length--; // = (this->hdma.length - 1) & 0xFF;
                    if (this->hdma.length <= 0) { // Terminate HDMA
                        this->hdma.enabled = false;
                        this->hdma.waiting = false;
                        this->hdma.completed = true;
                        this->hdma.active = false;
                    }
                    else {
                        this->hdma.completed = false;
                        this->hdma.enabled = true;
                        this->hdma.waiting = true;
                    }
                }
                this->hdma.til_next_byte = this->clock->turbo ? 2 : 1; // If we're at turbo-speed, we need to wait 2 M-cycles in between transfer. If not, 1.
            }
            return true; // we RAN so we RETURN TRUE
        }
    }
    return false;
}

static void GB_CPU_ghdma_run(struct GB_CPU* this)
{
    this->hdma.til_next_byte--;
    if (this->hdma.til_next_byte < 1) {
        // Copy byte
        GB_bus_CPU_write(this->bus, this->hdma.dest_index, GB_bus_CPU_read(this->bus, this->hdma.source_index, 0xFF, 1));
        this->hdma.dest_index = ((this->hdma.dest_index + 1) & 0x1FFF) | 0x8000;
        this->hdma.source_index = (this->hdma.source_index + 1) & 0xFFFF;
        // A 16-byte block has finished
        if ((this->hdma.dest_index & 0x0F) == 0) {
            this->hdma.length = (this->hdma.length - 1) & 0xFF;
            if (this->hdma.length == 0xFF) { // Terminate HDMA
                this->hdma.enabled = false;
                this->hdma.waiting = false;
                this->hdma.completed = true;
                this->hdma.active = false;
            }
        }
        this->hdma.til_next_byte = this->clock->turbo ? 2 : 1; // If we're at turbo-speed, we need to wait 2 M-cycles in between transfer. If not, 1.
    }
}

static u32 GB_CPU_hdma_eval(struct GB_CPU *this)
{
    if ((this->clock->cgb_enable) && (this->hdma.enabled)) {
        if (this->hdma.mode == 0) {
            GB_CPU_ghdma_run(this);
            return true;
        } else {
            return GB_CPU_hdma_run(this);
        }
    }
    return false;
}

void GB_CPU_switch_speed(struct GB_CPU* this)
{
    if (!this->clock->cgb_enable) return;
    if (this->clock->timing.cpu_divisor == 4) {
        this->clock->timing.cpu_divisor = 2;
        this->clock->turbo = true;
    }
    else {
        this->clock->timing.cpu_divisor = 4;
        this->clock->turbo = false;
    }
}

void GB_CPU_cycle(struct GB_CPU* this)
{
    this->clock->trace_cycles++;

    if (GB_CPU_hdma_eval(this)) {
        GB_timer_inc(&this->timer);
        return;
    }

    if ((this->cpu.regs.STP) && (this->io.speed_switch_prepare)) {
        if (this->io.speed_switch_cnt < 0) {
            this->io.speed_switch_cnt = (i32)(0x10000 - this->clock->SYSCLK); // Speed switchover actually happens at rollover of SYSCLK
        }
        this->io.speed_switch_cnt--;
        if (this->io.speed_switch_cnt == 0) {
            GB_CPU_switch_speed(this);
            this->io.speed_switch_prepare = 0;
            this->cpu.regs.TCU++;
            this->cpu.regs.STP = 0;
            this->clock->SYSCLK = 0;
        }
    }

    // Service CPU reads and writes
    if (this->cpu.pins.MRQ) {
        if (this->cpu.pins.RD) {
            this->cpu.pins.D = GB_bus_CPU_read(this->bus, this->cpu.pins.Addr, 0xCC, 1);
            if (this->tracing) {
                // TODO: debug
                //dbg.traces.add(TRACERS.SM83, this->cpu.trace_cycles, trace_format_read('SM83', SM83_COLOR, this->cpu.trace_cycles, this->cpu.pins.Addr, this->cpu.pins.D));
            }
        }
        if (this->cpu.pins.WR) {
            if ((!this->dma.running) || (this->cpu.pins.Addr >= 0xFF00))
                GB_bus_CPU_write(this->bus, this->cpu.pins.Addr, this->cpu.pins.D);

            if (this->tracing) {
                // TODO: debug
                //dbg.traces.add(TRACERS.SM83, this->cpu.trace_cycles, trace_format_write('SM83', SM83_COLOR, this->cpu.trace_cycles, this->cpu.pins.Addr, this->cpu.pins.D));
            }
        }
    }
    SM83_cycle(&this->cpu);
    GB_CPU_dma_eval(this);
    if (this->cpu.regs.STP)
        GB_timer_SYSCLK_change(&this->timer, 0);
    else {
        GB_timer_inc(&this->timer);
    }
    this->clock->SYSCLK = this->timer.SYSCLK;
}

void GB_CPU_quick_boot(struct GB_CPU* this)
{
    switch (this->variant) {
    case DMG:
        this->cpu.regs.A = 1;
        this->cpu.regs.F.Z = 0;
        this->cpu.regs.SP = 0xFFFE;
        this->cpu.regs.B = 0;
        this->cpu.regs.C = 0x13;
        this->cpu.regs.D = 0;
        this->cpu.regs.E = 0xD8;
        this->cpu.regs.H = 0x01;
        this->cpu.regs.L = 0x4D;
        this->cpu.regs.PC = 0x101;
        this->cpu.regs.TCU = 0;
        this->cpu.regs.IR = SM83_S_DECODE;
        this->cpu.regs.IME = 0;
        this->cpu.regs.IE = 0;
        this->cpu.regs.IF = 1;
        this->hdma.enabled = false;
        this->timer.TIMA = 0;
        this->timer.TMA = 0;
        this->timer.TAC = 0xF8;
        this->clock->bootROM_enabled = false;
        this->cpu.pins.Addr = 0x100;
        this->cpu.pins.MRQ = this->cpu.pins.RD = 1;
        this->cpu.pins.WR = 0;
        this->cpu.regs.poll_IRQ = true;
        break;
    case GBC:
        this->cpu.regs.A = 0x11;
        this->cpu.regs.F.Z = 1;
        //this->cpu.regs.F.N = this->cpu.regs.F.H = this->cpu.regs.F.C = 0;
        this->cpu.regs.SP = 0xFFFE;
        this->cpu.regs.B = 0;
        this->cpu.regs.C = 0;
        this->cpu.regs.D = 0xFF;
        this->cpu.regs.E = 0x56;
        this->cpu.regs.H = 0;
        this->cpu.regs.L = 0x0D;
        this->cpu.regs.PC = 0x101;
        this->cpu.regs.TCU = 0;
        this->cpu.regs.IR = SM83_S_DECODE;
        this->cpu.regs.IME = 0;
        this->cpu.regs.IE = 0;
        this->cpu.regs.IF = 1;
        this->timer.TIMA = 0;
        this->timer.TMA = 0;
        this->timer.TAC = 0xF8;
        this->clock->bootROM_enabled = false;
        this->hdma.dest = 0xFFFF;
        this->hdma.source = 0xFFFF;
        this->hdma.length = 0x7F;
        this->hdma.mode = 1;
        this->hdma.enabled = false;
        // TODO: mapper RESET
        /*
        this->bus.mapper.VRAM_bank_offset = 8192; // 0xFF startup register value
        this->bus.mapper.WRAM_bank_offset = 4096 * 7; // 0xFF startup register value
         */
        this->cpu.pins.Addr = 0x100;
        this->cpu.pins.MRQ = this->cpu.pins.RD = 1;
        this->cpu.pins.WR = 0;
        this->cpu.regs.poll_IRQ = true;
        break;
    default:
        printf("FAST BOOT NOT ENABLED FOR THIS VARIANT!");
        break;
    }
}

static void update_inputs(struct GB_CPU* this) {
    struct cvec* bl = &((struct physical_io_device *)cpg(this->device_ptr))->controller.digital_buttons;
    struct HID_digital_button* b;
#define B_GET(button, num) { b = cvec_get(bl, num); this->input_buffer. button = b->state; }
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

static u32 GB_CPU_get_input(struct GB_CPU* this) {
    u32 out1;
    u32 out3 = 0x0F;
    update_inputs(this);
    if (this->io.JOYP.action_select == 0) {
        out1 = this->input_buffer.a | (this->input_buffer.b << 1) | (this->input_buffer.select << 2) | (this->input_buffer.start << 3);
        out1 ^= 0x0F;
        out3 &= out1;
    }

    if (this->io.JOYP.direction_select == 0) {
        out1 = this->input_buffer.right | (this->input_buffer.left << 1) | (this->input_buffer.up << 2) | (this->input_buffer.down << 3);
        out1 ^= 0x0F;
        out3 &= out1;
    }
    return out3;
}


u32 GB_CPU_bus_read_IO(struct GB_bus *bus, u32 addr, u32 val)
{
    struct GB_CPU* this = bus->cpu;
    if ((addr >= 0xFF10) && (addr < 0xFF40)) {
        return GB_APU_read_IO(bus->apu, addr, val, 1);
    }
    switch(addr) {
        case 0xFF00: // JOYP
            // return not pressed=1 in bottom 4 bits
            return GB_CPU_get_input(this) | (this->io.JOYP.action_select << 4) | (this->io.JOYP.direction_select << 5) | 0xC0;
        case 0xFF01: // SB serial
            //return this->FFregs[1];
            return 0xFF;
        case 0xFF02: // SC
            return val;
        case 0xFF04: // DIV
        case 0xFF05: // TIMA
        case 0xFF06: // TIMA reload
        case 0xFF07: // TAC timer control
            return GB_timer_read_IO(&this->timer, addr);
        case 0xFF0F: // IF: interrupt flag
            //return this->cpu.regs.IF & 0x1F;
            return this->cpu.regs.IF | 0xE0;
            //return this->clock->irq.vblank_request | (this->clock->irq.lcd_stat_request << 1) | (this->clock->irq.timer_request << 2) | (this->clock->irq.serial_request << 3) | (this->clock->irq.joypad_request << 4);
        case 0xFF46: // OAM DMA
            return this->dma.last_write;
        case 0xFF4D: // Speed switch enable
            if (!this->clock->cgb_enable) return 0xFF;
            return this->io.speed_switch_prepare | (this->clock->turbo ? 0x80 : 0);
        case 0xFF4F: // VRAM bank
            if (!this->clock->cgb_enable) return 0xFF;
            return this->bus->VRAM_bank;
        case 0xFF51: // HDMA1 MSB
            if (!this->clock->cgb_enable) return 0xFF;
            return (this->hdma.source & 0xFF00) >> 8;
        case 0xFF52: // HDMA2 LSB
            if (!this->clock->cgb_enable) return 0xFF;
            return this->hdma.source & 0xFF;
        case 0xFF53: // HDMA3 MSB
            if (!this->clock->cgb_enable) return 0xFF;
            return (this->hdma.dest & 0xFF00) >> 8;
        case 0xFF54: // HDMA4 LSB
            if (!this->clock->cgb_enable) return 0xFF;
            return this->hdma.dest & 0xFF;
        case 0xFF55: // HDMA5
            if (!this->clock->cgb_enable) return 0xFF;
            return (this->hdma.enabled ? 0 : 0x80) | this->hdma.length;
        case 0xFF70: // WRAM bank
            if (!this->clock->cgb_enable) return 0xFF;
            return this->bus->WRAM_bank;
        case 0xFFFF: // IE Interrupt Enable
            //return this->cpu.regs.IE & 0x1F;
            return this->cpu.regs.IE | 0xE0;
            //return this->clock->irq.vblank_enable | (this->clock->irq.lcd_stat_enable << 1) | (this->clock->irq.timer_enable << 2) | (this->clock->irq.serial_enable << 3) | (this->clock->irq.joypad_enable << 4);
    }
    //if (addr == 0xFF44) printf("\nHEY VAL GOES OUT %02x", val);
    return val;
    // TODO: reenable APU reads here
    //return this->bus->APU_read_IO(addr, val);
}

void GB_CPU_bus_write_IO(struct GB_bus* bus, u32 addr, u32 val)
{
    struct GB_CPU* this = bus->cpu;
    if ((addr >= 0xFF10) && (addr < 0xFF40)) {
        GB_APU_write_IO(bus->apu, addr, val);
        return;
    }
    switch(addr) {

        case 0xFF00: // JOYP
            this->io.JOYP.action_select = (val & 0x20) >> 5;
            this->io.JOYP.direction_select = (val & 0x10) >> 4;
            return;
        case 0xFF01: // SB serial
            this->FFregs[1] = val;
            return;
        case 0xFF02: // SC
            this->FFregs[2] = val;
            //this->cycles_til_serial_interrupt =
            return;
        case 0xFF04: // DIV
        case 0xFF05: // TIMA
        case 0xFF06: // TIMA reload
        case 0xFF07: // TAC TIMA controler
            GB_timer_write_IO(&this->timer, addr, val);
            return;
        case 0xFF46: // OAM DMA
            if (GB_INSTANT_OAM) {
                this->dma.high = (val << 8);
                for (u32 i = 0; i < 160; i++) {
                    this->bus->write_OAM(this->bus, 0xFE00 | i, GB_bus_CPU_read(this->bus, this->dma.high | i, 0, 1));
                }
            } else {
                this->dma.cycles_til = 2;
                ////this->dma.running = 1;
                this->dma.new_high = (val << 8);
                this->dma.last_write = val;
            }
            return;
        case 0xFF4D: // Speed switch enable
            if (!this->clock->cgb_enable) return;
            //console.log('PREPARE SPEED SWITCH');
            this->io.speed_switch_prepare = val & 1;
            return;
        case 0xFF4F: // VRAM bank
            if (!this->clock->cgb_enable) return;
            this->bus->VRAM_bank = val;
            this->bus->generic_mapper.VRAM_bank_offset = 8192 * (val & 1);
            return;
        case 0xFF50: // Boot ROM disable. Cannot re-enable
            if (val & 1) {
                printf("Disable boot ROM!");
                //dbg.break();
                this->clock->bootROM_enabled = false;
            }
            return;
        case 0xFF51: // HDMA1   bits 15-8 of dest
            if (!this->clock->cgb_enable) return;
            //this->hdma.source = (this->hdma.source & 0xF0) | (val << 8);
            this->hdma.source = (this->hdma.source & 0xFF) | (val << 8);
            return;
        case 0xFF52: // HDMA2   bits 7-4 of dest
            if (!this->clock->cgb_enable) return;
            this->hdma.source = (this->hdma.source & 0xFF00) | val;
            //this->hdma.source = (this->hdma.source & 0xFF00) | (val & 0xF0);
            return;
        case 0xFF53: // HDMA3  bits 12-8 of dest
            if (!this->clock->cgb_enable) return;
            this->hdma.dest = (this->hdma.dest & 0xFF) | (val << 8);
            //this->hdma.dest = 0x8000 | (this->hdma.dest & 0x1F00) | (val & 0xF0);
            return;
        case 0xFF54: // HDMA4 bits
            if (!this->clock->cgb_enable) return;
            this->hdma.dest = (this->hdma.dest & 0xFF00) | val;
            //this->hdma.dest = 0x8000 | (this->hdma.dest & 0xF0) | ((val & 0x1F) << 8);
            return;
        case 0xFF55: // HDMA5 transfer stuff!
            if (!this->clock->cgb_enable) return;
            //console.log('HDMA5 TRANSFER START', this->clock->trace_cycles, hex2(val));
            // if LCD ON...
            if (this->bus->ppu->enabled) {
                this->hdma.notify_hblank = (this->clock->ppu_mode == 0);
            } else {// if LCD OFF... {
                this->hdma.notify_hblank = true;
                //console.log('LCD OFF SO NOTIFY HBLANK!');
            }
            this->hdma.mode = (val & 0x80) >> 7;
            this->hdma.length = (val & 0x7F) + 1; // up to 128 blocks of 16 bytes.
            if (!this->hdma.enabled) {
                printf("\nHDMA TRANSFER");
                this->hdma.enabled = true;
                if (this->hdma.mode == 0) this->hdma.active = true;
            } else {
                if ((val & 0x80) == 0) {
                    //console.log('HDMA TRANSFER CANCEL!');
                    this->hdma.enabled = false;
                    this->hdma.active = false;
                }
            }
            this->hdma.dest_index = (this->hdma.dest & 0x1FF0) | 0x8000;
            this->hdma.source_index = this->hdma.source & 0xFFF0;
            //this->hdma.last_ly = 250;

            this->hdma.til_next_byte = this->clock->turbo ? 2 : 1; // If we're at turbo-speed, we need to wait 2 M-cycles in between transfer. If not, 1.
            return;
        case 0xFF6C:
            if (!this->clock->cgb_enable) return;
            printf("OBJPRIOR WRITE! %d", val);
            return;
        case 0xFF70: // WRAM bank
            if (!this->clock->cgb_enable) return;
            this->bus->WRAM_bank = val;
            val &= 7;
            if (val == 0) val = 1;
            this->bus->generic_mapper.WRAM_bank_offset = 4096 * val;
            return;
        case 0xFF0F:
            //console.log('WRITE IF', val & 0x1F);
            this->cpu.regs.IF = val & 0x1F;
            return;
        case 0xFFFF: // IE: Interrupt Enable
            this->cpu.regs.IE = val & 0x1F;
            return;
    }
    //TODO: APU stuff
    //this->bus->apu.write_IO(addr, val);
}
