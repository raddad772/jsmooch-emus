//
// Created by . on 4/21/25.
//

#include <string.h>
#include "snes_bus.h"
#include "r5a22.h"

void R5A22_init(struct R5A22 *this, u64 *master_cycle_count)
{
    memset(this, 0, sizeof(*this));
    WDC65816_init(&this->cpu, master_cycle_count);
    for (u32 i = 0; i < 7; i++) {
        this->dma.channels[i].next = &this->dma.channels[i+1];
    }
}

static void dma_reset(struct R5A22 *this)
{
    for (u32 i = 0; i < 8; i++) {
        struct R5A22_DMA_CHANNEL *ch = &this->dma.channels[i];
        memset(ch, 0, sizeof(*ch));
        if (i < 7) ch->next = &this->dma.channels[i+1];
    }
}

void R5A22_reset(struct R5A22 *this)
{
    WDC65816_reset(&this->cpu);
    this->cpu.regs.TCU = 0;
    this->cpu.pins.D = WDC65816_OP_RESET;
    dma_reset(this);
    this->ROMspeed = 8;
    this->status.auto_joypad_counter = 33;

    this->io.wrmpya = 0xFF;   // $4202
    this->io.wrmpyb = 0xFF;   // $4203
    this->io.wrdiva = 0xFFFF; // $4204-4205
    this->io.wrdivb = 0xFF;   // $4026

    this->io.rddiv = 0; // $4214-4215
    this->io.rdmpy = 0; // $4216-4217
    this->io.htime = 0x1FF; // HIRQ time
    this->io.vtime = 0x1FF; // VIRQ time
}

void R5A22_delete(struct R5A22 *this)
{
    WDC65816_delete(&this->cpu);
}

static u32 dma_reg_read(struct SNES *this, u32 addr, u32 old, u32 has_effect)
{
    printf("\nWARN DMA REG READ NOT DONE YET %04x", addr);
    return 0;
}


static void dma_reg_write(struct SNES *this, u32 addr, u32 val)
{
    printf("\nWARN DMA REG WRITE NOT DONE YET %04x %02x", addr, val);

}

void R5A22_update_irq(struct SNES *snes) {
    struct R5A22 *this = &snes->r5a22;
    // HIRQs are handled in a
    u32 old_h_irq = this->status.hirq_status;
    u32 old_v_irq = this->status.virq_status;

    this->status.hirq_status = this->status.hirq_line && this->io.hirq_enable;
    this->status.virq_status = this->io.virq_enable && (snes->clock.ppu.y == this->io.vtime);

    if (this->io.hirq_enable && this->io.virq_enable) { // H & V
        this->status.irq_line = this->status.hirq_status && this->status.virq_status;
        if (!old_h_irq && !old_v_irq && this->status.irq_line) { // If we're firing...
            this->status.irq_flag = 1;
        }
    } else { // H or V
        // If we're firing...
        if ((!old_h_irq && this->status.hirq_status) || (!old_v_irq && this->status.virq_status)) {
            this->status.irq_flag = 1;
        }
        this->status.irq_line = this->status.hirq_status || this->status.virq_status;
    }

    this->status.irq_line &= this->status.irq_flag;

    WDC65816_set_IRQ_level(&this->cpu, this->status.irq_line);
}

void R5A22_update_nmi(struct SNES *snes)
{
    struct R5A22 *this = &snes->r5a22;
    WDC65816_set_NMI_level(&this->cpu, this->io.nmi_enable && this->status.nmi_flag);
}

static void latch_ppu_counters(struct SNES *snes)
{
    struct R5A22 *this = &snes->r5a22;
    snes->ppu.io.vcounter = snes->clock.ppu.y;
    snes->ppu.io.hcounter = (snes->clock.master_cycle_count - snes->clock.ppu.scanline_start) >> 2;
    snes->ppu.latch.counters = 1;
}

static void hdma_pending_setup(void *ptr, u64 key, u64 clock, u32 jitter)
{
    // TODO: what!?
    struct SNES *snes = (struct SNES *)ptr;
    struct R5A22 *this = &snes->r5a22;

    snes->clock.dma_counter = 0;
    if (!this->status.dma_running) {
        snes->clock.dma_counter += (snes->clock.master_cycle_count & 7);
    }
    this->status.hdma_pending = 0;
    //hdma_run(snes);
    scheduler_from_event_adjust_master_clock(&snes->scheduler, snes->clock.dma_counter);
}

static void dma_start(struct SNES *snes)
{
    struct R5A22 *this = &snes->r5a22;
    for (u32 n = 0; n < 8; n++) {
        this->dma.channels[n].index = 0;
    }
}

static void dma_pending_setup(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (struct SNES *)ptr;
    struct R5A22 *this = &snes->r5a22;

    if (!this->status.dma_running) dma_start(snes);
    this->status.dma_pending = 0;
    this->status.dma_running = 1;
}

void R5A22_reg_write(struct SNES *snes, u32 addr, u32 val, struct SNES_memmap_block *bl)
{
    if ((addr >= 0x4300) && (addr <= 0x43FF)) { return dma_reg_write(snes, addr, val); return; }
    struct R5A22 *this = &snes->r5a22;

    switch(addr) {
        case 0x4016:
            // TODO: controller latch;
            break;
        case 0x4200: // IRQ control
            this->io.auto_joypad_poll = val & 1;
            if (!this->io.auto_joypad_poll) this->status.auto_joypad_counter = 33;
            this->io.hirq_enable = (val >> 4) & 1;
            this->io.virq_enable = (val >> 5) & 1;
            this->io.irq_enable = this->io.hirq_enable || this->io.virq_enable;
            this->io.nmi_enable = (val >> 7) & 1;
            R5A22_update_irq(snes);
            R5A22_update_nmi(snes);
            return;
        case 0x4201: // WRIO, a weird one
            if ((!(this->io.pio & 0x80)) && !(val & 0x80)) latch_ppu_counters(snes);
            this->io.pio = val;
            return;
        case 0x4202:
            this->io.wrmpya = val;
            return;
        case 0x4203:
            this->io.wrmpyb = val;
            if (this->alu.mpyctr || this->alu.divctr) return;
            this->io.wrmpyb = val;
            this->io.rddiv = (this->io.wrmpyb << 8) | this->io.wrmpya;
            this->alu.mpyctr = 8;
            this->alu.shift = this->io.wrmpyb;
            return;
        case 0x4204: // Dividend lo
            this->io.wrdiva = (this->io.wrdiva & 0xFF00) + val;
            return;
        case 0x4205:
            this->io.wrdiva = (this->io.wrdiva & 0xFF) + (val << 8);
            return;
        case 0x4206:
            this->io.rdmpy = this->io.wrdiva;
            if(this->alu.mpyctr || this->alu.divctr) return;

            this->io.wrdivb = val;

            this->alu.divctr = 16;
            this->alu.shift = this->io.wrdivb << 16;
            return;
        case 0x4207:
            this->io.htime = (this->io.htime >> 2) - 1;
            this->io.htime = (this->io.htime & 0x100) | val;
            this->io.htime = (this->io.htime + 1) << 2;
            return;
        case 0x4208: // HTIME hi
            this->io.htime = (this->io.htime >> 2) - 1;
            this->io.htime = (this->io.htime & 0xFF) | ((val & 1) << 8);
            this->io.htime = (this->io.htime + 1) << 2;
            return;
        case 0x4209: // VTIME lo
            this->io.vtime = (this->io.vtime & 0x100) | val;
            return;
        case 0x420A: // VTIME hi
            this->io.vtime = (this->io.vtime & 0xFF) | ((val & 1) << 8);
            return;
        case 0x420B:
            for (u32 n = 0; n < 8; n++) {
                this->dma.channels[n].dma_enable = (val >> n) & 1;
            }
            if (val != 0) {
                if (this->dma.sched.still)
                    scheduler_delete_if_exist(&snes->scheduler, this->dma.sched.id);
                this->dma.sched.id = scheduler_add_next(&snes->scheduler, 0, snes, &dma_pending_setup, &this->dma.sched.still);
            }
            return;
        case 0x420C:
            this->dma.hdma_enabled = val ? 1 : 0;
            for (u32 n = 0; n < 8; n++) {
                this->dma.channels[n].hdma_enable = (val >> n) & 1;
            }
            //if (val !== 0) console.log('HDMA CHANNEL WRITE', val, this->dma.channels[7]);
            return;
        case 0x420D:
            this->ROMspeed = (val & 1) ? 6 : 8;
            return;

    }

    printf("\nR5A22 MISS WRITE TO %04x %02x", addr, val);
}

void R5A22_hblank(struct SNES *snes, u32 which)
{
    // evaluate hblank dma
    if (which) {

    }
}

u32 R5A22_reg_read(struct SNES *snes, u32 addr, u32 old, u32 has_effect, struct SNES_memmap_block *bl)
{
    if ((addr >= 0x4300) && (addr <= 0x43FF)) return dma_reg_read(snes, addr, old, has_effect );
    struct R5A22 *this = &snes->r5a22;
    u32 val;

    switch(addr) {
        case 0x4016: // controller 1 data
            return 0xFF;
        case 0x4017: // controller 2 data
            return 0xFF;
        case 0x4210: // NMI/version read
            val = old & 0x70;
            val |= this->status.nmi_flag << 7;
            if (has_effect) this->status.nmi_flag = 0;
            val |= snes->clock.rev;
            return val;
        case 0x4211:
            val = old & 0x7F;
            val |= this->status.irq_flag << 7;
            if (has_effect) this->status.irq_flag = 0;
            return val;
        case 0x4212:
            val = this->io.auto_joypad_poll && this->status.auto_joypad_counter < 33;
            val |= snes->clock.ppu.hblank_active << 6;
            val |= snes->clock.ppu.vblank_active << 7;
            return val;
        case 0x4213: // JOYSER1
            return this->io.pio;
        case 0x4214: // Hardware multiplier stuff
            return this->io.rddiv & 0xFF;
        case 0x4215:
            return (this->io.rddiv >> 8) & 0xFF;
        case 0x4216:
            return this->io.rdmpy & 0xFF;
        case 0x4217:
            return (this->io.rdmpy >> 8) & 0xFF;
        case 0x4218:
            return this->io.joy1 & 0xFF;
        case 0x4219:
            return ((this->io.joy1) >> 8) & 0xFF;
        case 0x421A:
            return this->io.joy2 & 0xFF;
        case 0x421B:
            return ((this->io.joy2) >> 8) & 0xFF;
        case 0x421C:
            return this->io.joy3 & 0xFF;
        case 0x421D:
            return ((this->io.joy3) >> 8) & 0xFF;
        case 0x421E:
            return this->io.joy4 & 0xFF;
        case 0x421F:
            return ((this->io.joy4) >> 8) & 0xFF;
    }
    printf("\nR5A22 MISS READ TO %04x", addr);
    return 0;
}

void R5A22_setup_tracing(struct R5A22* this, struct jsm_debug_read_trace *strct)
{
    WDC65816_setup_tracing(&this->cpu, strct);
}

static u32 mem_timing(u32 addr, u32 ROMspeed) {
    // Taken from a byuu post on a forum, thanks byuu!
    // Determine CPU cycle length in master clocks based on address currently in use (speed is 6 if no address being read/written)

    u32 rspeed = ROMspeed ? ROMspeed : 8;

    // 00-3f, 80-bf:8000-ffff; 40-7f, c0-ff:0000-ffff
    if (addr & 0x408000) return addr & 0x800000 ? rspeed : 8;
    // 00-3f,80-bf:0000-1fff, 6000-7fff
    if (addr + 0x6000 & 0x4000) return 8;
    // 00-3f, 80-bf:2000-3fff, 4200-5fff
    if (addr - 0x4000 & 0x7e00) return 6;

    //00-34,80-bf:4000-41ff
    return 12;
}

static void cycle_alu(struct SNES *snes)
{
    struct R5A22 *this = &snes->r5a22;
    if (!this->alu.mpyctr  && !this->alu.divctr) return;
    if (this->alu.mpyctr) {
        this->alu.mpyctr--;
        if (this->io.rddiv & 1) this->io.rdmpy += this->alu.shift;
        this->io.rddiv >>= 1;
        this->alu.shift <<= 1;
    }

    if (this->alu.divctr) {
        this->alu.divctr--;
        this->io.rddiv <<= 1;
        this->alu.shift >>= 1;
        if (this->io.rdmpy >= this->alu.shift) {
            this->io.rdmpy -= this->alu.shift;
            this->io.rddiv |= 1;
        }
    }
}

static void cycle_cpu(struct SNES *snes)
{
    struct R5A22 *this = &snes->r5a22;
    WDC65816_cycle(&this->cpu);
    u32 cpu_addr = (this->cpu.pins.BA << 16) | this->cpu.pins.Addr;
    snes->clock.cpu.divider = this->cpu.pins.PDV ? mem_timing(cpu_addr, this->ROMspeed) : 6;

    if (this->cpu.pins.PDV) { // Read/write. THIS ONLY WORKS FOR PDV MODE not expanded pins
        if (this->cpu.pins.RW) { // Write
            SNES_wdc65816_write(snes, cpu_addr, this->cpu.pins.D);
        }
        else { // Read
            this->cpu.pins.D = SNES_wdc65816_read(snes, cpu_addr, this->cpu.pins.D, 1);
        }
    }
}

static u32 dma_run_ch(struct SNES *snes, struct R5A22_DMA_CHANNEL *this)
{
    u32 nc = 0;
    if (this->transfer_size > 0) {
        if (this->index == 0) { // 8 cycles for setup
            nc += 8;
        }
        //dma_transfer(snes, this, (this->source_bank << 16) | this->source_address, this->index);
        this->index++;
        nc += 8;
        if (!this->fixed_transfer) {
            if (this->reverse_transfer)
                this->source_address--;
            else
                this->source_address++;
            this->source_address &= 0xFFFF;
        }
        this->transfer_size--;
    }
    this->dma_enable = this->transfer_size > 0;

    scheduler_from_event_adjust_master_clock(&snes->scheduler, nc);
    return 0;
}

static u32 dma_run(struct SNES *snes)
{
    struct R5A22 *this = &snes->r5a22;
    u32 any_going = 0;
    u32 howmany = 0;
    for (u32 n = 0; n < 8; n++) {
        struct R5A22_DMA_CHANNEL *ch = &this->dma.channels[n];
        if (ch->dma_enable) {
            any_going++;
            dma_run_ch(snes, ch);
        }
    }
    if (!any_going) this->status.dma_running = 0;
    return 0;
}

static void R5A22_cycle(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (struct SNES *)ptr;
    struct R5A22 *this = &snes->r5a22;
    u64 cur = clock - jitter;

    if (this->status.dma_running) dma_run(snes);
    else cycle_cpu(snes);
    cycle_alu(snes);
    scheduler_only_add_abs(&snes->scheduler, cur + snes->clock.cpu.divider, 0, snes, &R5A22_cycle, NULL);
}

void R5A22_schedule_first(struct SNES *snes)
{
    scheduler_only_add_abs(&snes->scheduler, 8, 0, snes, &R5A22_cycle, NULL);
}
