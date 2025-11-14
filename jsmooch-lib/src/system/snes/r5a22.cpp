//
// Created by . on 4/21/25.
//

#include <cstring>
#include "snes_bus.h"
#include "r5a22.h"
#include "snes_debugger.h"

u32 R5A22_DMA_CHANNEL::hdma_is_active()
{
    return hdma_enable && !hdma_completed;
}

u32 R5A22_DMA_CHANNEL::hdma_is_finished()
{
    R5A22_DMA_CHANNEL *ch = next;
    while(ch != nullptr) {
        if (ch->hdma_is_active()) return 0;
        ch = ch->next;
    }
    return true;

}

u32 R5A22_DMA_CHANNEL::hdma_reload_ch()
{
    u32 cn = 8;
    u32 data = SNES_wdc65816_read(snes, (source_bank << 16 | hdma_address), 0, 1);
    if ((line_counter & 0x7F) == 0) {
        line_counter = data;
        hdma_address++;

        hdma_completed = line_counter == 0;
        hdma_do_transfer = !hdma_completed;

        if (indirect) {
            cn += 8;
            data = SNES_wdc65816_read(snes, (source_bank << 16) | hdma_address, 0, 1);
            hdma_address++;
            indirect_address = data << 8;
            if (hdma_completed && hdma_is_finished(ch)) return cn;

            cn += 8;
            data = SNES_wdc65816_read(snes, (source_bank << 16) | hdma_address, 0, 1);
            hdma_address++;

            indirect_address = (data << 8) | (indirect_address >> 8);
        }
    }
    return cn;
}

u32 R5A22::hdma_is_enabled()
{
    for (u32 n = 0; n < 8; n++) {
        if (dma.channels[n].hdma_enable) return 1;
    }
    return 0;
}


u32 R5A22_DMA_CHANNEL::hdma_setup_ch()
{
    hdma_do_transfer = 1;
    if (!hdma_enable) return 0;

    dma_enable = 0; // Stomp on DMA if HDMA runs
    hdma_address = source_address;
    //line_counter = 0;
    return hdma_reload_ch();
}

u32 R5A22_DMA_CHANNEL::dma_run_ch()
{
    u32 nc = 0;
    if (transfer_size > 0) {
        if (index == 0) { // 8 cycles for setup
            nc += 8;
        }
        dma_transfer(snes, this, (source_bank << 16) | source_address, index, 0);
        index++;
        nc += 8;
        if (!fixed_transfer) {
            if (reverse_transfer)
                source_address--;
            else
                source_address++;
            source_address &= 0xFFFF;
        }
        transfer_size--;
    }
    dma_enable = transfer_size > 0;
    if (!dma_enable) {
        dbgloglog(snes, SNES_CAT_DMA_WRITE, DBGLS_INFO, "DMA%d END!", num);
    }

    if (nc) {
        scheduler_from_event_adjust_master_clock(&snes->scheduler, nc);
    }
    return 0;
}


void R5A22_init(R5A22 *this, u64 *master_cycle_count)
{
    memset(this, 0, sizeof(*this));
    WDC65816_init(&cpu, master_cycle_count);
    for (u32 i = 0; i < 8; i++) {
        dma.channels[i].num = i;
        if (i < 7) dma.channels[i].next = &dma.channels[i+1];
    }
}

static void dma_reset(R5A22 *this)
{
    for (u32 i = 0; i < 8; i++) {
        R5A22_DMA_CHANNEL *ch = &dma.channels[i];
        memset(ch, 0, sizeof(*ch));
        ch->num = i;
        if (i < 7) ch->next = &dma.channels[i+1];
    }
}

void R5A22_reset(R5A22 *this)
{
    WDC65816_reset(&cpu);
    cpu.regs.TCU = 0;
    cpu.pins.D = WDC65816_OP_RESET;
    dma_reset(this);
    ROMspeed = 8;
    status.auto_joypad.counter = 33;

    io.wrmpya = 0xFF;   // $4202
    io.wrmpyb = 0xFF;   // $4203
    io.wrdiva = 0xFFFF; // $4204-4205
    io.wrdivb = 0xFF;   // $4026

    io.rddiv = 0; // $4214-4215
    io.rdmpy = 0; // $4216-4217
    io.htime = 0x1FF; // HIRQ time
    io.vtime = 0x1FF; // VIRQ time
}

void R5A22_delete(R5A22 *this)
{
    WDC65816_delete(&cpu);
}

static u32 dma_reg_read(SNES *snes, u32 addr, u32 old, u32 has_effect)
{
    R5A22_DMA_CHANNEL *ch = &snes->r5a22.dma.channels[(addr >> 4) & 7];
    switch(addr & 0xFF8F) {
        case 0x4300: // DMAPx
            return (ch->transfer_mode) | (ch->fixed_transfer << 3) | (ch->reverse_transfer << 4) | (ch->unused << 5) | (ch->indirect << 6) || (ch->direction << 7);
        case 0x4301:
            return ch->target_address;
        case 0x4302:
            return ch->source_address & 0xFF;
        case 0x4303:
            return (ch->source_address >> 8) & 0xFF;
        case 0x4304:
            return ch->source_bank;
        case 0x4305:
            return ch->transfer_size & 0xFF;
        case 0x4306:
            return (ch->transfer_size >> 8) & 0xFF;
        case 0x4307:
            return ch->indirect_bank;
        case 0x4308:
            return ch->hdma_address & 0xFF;
        case 0x4309:
            return (ch->hdma_address >> 8) & 0xFF;
        case 0x430a:
            return ch->line_counter;
        case 0x430b:
            return ch->unknown_byte;
        case 0x430f:
            return ch->unknown_byte;
        
    }
    return old;
}


static void dma_reg_write(SNES *snes, u32 addr, u32 val)
{
    u32 cnum = (addr >> 4) & 7;
    R5A22_DMA_CHANNEL *ch = &snes->r5a22.dma.channels[cnum];
    switch(addr & 0xFF8F) {
        case 0x4300: // DMAPx various controls
            ch->transfer_mode = val & 7;
            ch->fixed_transfer = (val >> 3) & 1;
            ch->reverse_transfer = (val >> 4) & 1;
            ch->unused = (val >> 5) & 1;
            ch->indirect = (val >> 6) & 1;
            ch->direction = (val >> 7) & 1;
            return;
        case 0x4301:
            ch->target_address = val;
            return;
        case 0x4302:
            ch->source_address = (ch->source_address & 0xFF00) | val;
            return;
        case 0x4303:
            ch->source_address = (val << 8) | (ch->source_address & 0xFF);
            return;
        case 0x4304:
            ch->source_bank = val;
            return;
        case 0x4305:
            ch->transfer_size = (ch->transfer_size & 0xFF00) | val;
            return;
        case 0x4306:
            ch->transfer_size = (val << 8) | (ch->transfer_size & 0xFF);
            return;
        case 0x4307:
            ch->indirect_bank = val;
            return;
        case 0x4308:
            ch->hdma_address = (ch->hdma_address & 0xFF00) | val;
            return;
        case 0x4309:
            ch->hdma_address = (val << 8) | (ch->hdma_address & 0xFF);
            return;
        case 0x430A:
            ch->line_counter = val;
            return;
        case 0x430B:
            ch->unknown_byte = val;
            return;
        case 0x430F:
            ch->unknown_byte = val;
            return;
    }
}

void R5A22::update_irq() {
    // HIRQs are handled in a
    u32 old_h_irq = status.hirq_status;
    u32 old_v_irq = status.virq_status;

    status.hirq_status = status.hirq_line && io.hirq_enable;
    status.virq_status = io.virq_enable && (snes->clock.ppu.y == io.vtime);

    if (io.hirq_enable && io.virq_enable) { // H & V
        status.irq_line = status.hirq_status && status.virq_status;
        if (!old_h_irq && !old_v_irq && status.irq_line) { // If we're firing...
            status.irq_flag = 1;
            printf("\nHVIRQ FLAG LINE:%d CYCLE:%lld", snes->clock.ppu.y, snes->clock.master_cycle_count - snes->clock.ppu.scanline_start);
        }
    } else { // H or V
        // If we're firing...
        if ((!old_h_irq && status.hirq_status) || (!old_v_irq && status.virq_status)) {
            status.irq_flag = 1;
            printf("\nIRQ FLAG LINE:%d CYCLE:%lld", snes->clock.ppu.y, snes->clock.master_cycle_count - snes->clock.ppu.scanline_start);
        }
        status.irq_line = status.hirq_status || status.virq_status;
    }

    status.irq_line &= status.irq_flag;

    WDC65816_set_IRQ_level(&cpu, status.irq_line);
}

void R5A22::update_nmi()
{
    cpu.set_NMI_level(io.nmi_enable && status.nmi_flag);
}

static void dma_start(SNES *snes)
{
    R5A22 *this = &snes->r5a22;
    for (u32 n = 0; n < 8; n++) {
        R5A22_DMA_CHANNEL *ch = &dma.channels[n];
        if (ch->dma_enable) {
            dbgloglog(snes, SNES_CAT_DMA_START, DBGLS_INFO, "DMA%d start. size:%d line:%d addrA:%06x addrB:%04x vram_addr:%04x", n, ch->transfer_size, snes->clock.ppu.y, (ch->source_bank << 16) | ch->source_address, 0x2100 | ch->target_address,  snes->ppu.io.vram.addr);
        }
        ch->index = 0;
    }
}

static void dma_pending_setup(void *ptr, u64 key, u64 clock, u32 jitter)
{
    SNES *snes = (SNES *)ptr;
    R5A22 *this = &snes->r5a22;

    if (!status.dma_running) {
        dma_start(snes);
    }
    status.dma_pending = 0;
    status.dma_running = 1;
}

static void auto_joypad_edge(void *ptr, u64 key, u64 clock, u32 jitter)
{
    SNES *snes = (SNES *)ptr;
    R5A22 *this = &snes->r5a22;

    if (!io.auto_joypad_poll) return;
    u64 cur = clock - jitter;
    status.auto_joypad.sch_id = scheduler_only_add_abs(&snes->scheduler, cur + 128, 0, snes, &auto_joypad_edge, &status.auto_joypad.still_sched);

    u64 hcounter = (cur - snes->clock.ppu.scanline_start) >> 2;
    if ((snes->clock.ppu.y == 223) && (hcounter >= 130) && (hcounter <= 256)) {
        status.auto_joypad.counter = 0;
    }
    if (status.auto_joypad.counter >= 33) return;

    if (status.auto_joypad.counter == 0) {
        SNES_controllerport_latch(snes, 0, 1);
        SNES_controllerport_latch(snes, 1, 1);
    }

    if (status.auto_joypad.counter == 1) {
        SNES_controllerport_latch(snes, 0, 0);
        SNES_controllerport_latch(snes, 1, 0);

        io.joy1 = io.joy2 = io.joy3 = io.joy4 = 0;
    }

    if (status.auto_joypad.counter >= 2 && (!(status.auto_joypad.counter & 1))) {
        u32 p0 = SNES_controllerport_data(snes, 0);
        u32 p1 = SNES_controllerport_data(snes, 1);

        io.joy1 = ((io.joy1 << 1) | (p0 & 1)) & 0xFFFF;
        io.joy2 = ((io.joy2 << 1) | (p1 & 1)) & 0xFFFF;
        io.joy3 = ((io.joy3 << 1) | ((p0 >> 1) & 1)) & 0xFFFF;
        io.joy4 = ((io.joy4 << 1) | ((p1 >> 1) & 1)) & 0xFFFF;
    }
    status.auto_joypad.counter++;

}

void R5A22_reg_write(SNES *snes, u32 addr, u32 val, SNES_memmap_block *bl)
{
    addr &= 0xFFFF;
    if ((addr >= 0x4300) && (addr <= 0x43FF)) { return dma_reg_write(snes, addr, val); return; }
    R5A22 *this = &snes->r5a22;

    switch(addr) {
        case 0x4016:
            SNES_controllerport_latch(snes, 0, 0);
            SNES_controllerport_latch(snes, 1, 0);
            return;
        case 0x4200: {// IRQ control
            u32 old_joypad_poll = io.auto_joypad_poll;
            io.auto_joypad_poll = val & 1;
            if (old_joypad_poll && !io.auto_joypad_poll) { // unschedule...
                if (status.auto_joypad.still_sched)
                    scheduler_delete_if_exist(&snes->scheduler, status.auto_joypad.sch_id);
            }
            if (io.auto_joypad_poll) {
                if (!status.auto_joypad.still_sched) {
                    status.auto_joypad.sch_id = scheduler_only_add_abs(&snes->scheduler, (snes->clock.master_cycle_count & ~127) + 128, 0, snes, &auto_joypad_edge, &status.auto_joypad.still_sched);
                }
            }
            if (!io.auto_joypad_poll) status.auto_joypad.counter = 33;
            io.hirq_enable = (val >> 4) & 1;
            io.virq_enable = (val >> 5) & 1;
            io.irq_enable = io.hirq_enable || io.virq_enable;
            io.nmi_enable = (val >> 7) & 1;
            R5A22_update_irq(snes);
            R5A22_update_nmi(snes);
            return; }
        case 0x4201: // WRIO, a weird one
            if ((!(io.pio & 0x80)) && !(val & 0x80)) SNES_latch_ppu_counters(snes);
            io.pio = val;
            return;
        case 0x4202:
            io.wrmpya = val;
            return;
        case 0x4203:
            io.rdmpy = 0;
            if (alu.mpyctr || alu.divctr) return;
            io.wrmpyb = val;
            io.rddiv = (io.wrmpyb << 8) | io.wrmpya;
            alu.mpyctr = 8;
            alu.shift = io.wrmpyb;
            return;
        case 0x4204: // Dividend lo
            io.wrdiva = (io.wrdiva & 0xFF00) + val;
            return;
        case 0x4205:
            io.wrdiva = (io.wrdiva & 0xFF) + (val << 8);
            return;
        case 0x4206:
            io.rdmpy = io.wrdiva;
            if(alu.mpyctr || alu.divctr) return;

            io.wrdivb = val;

            alu.divctr = 16;
            alu.shift = io.wrdivb << 16;
            return;
        case 0x4207:
            io.htime = (io.htime >> 2) - 1;
            io.htime = (io.htime & 0x100) | val;
            io.htime = (io.htime + 1) << 2;
            return;
        case 0x4208: // HTIME hi
            io.htime = (io.htime >> 2) - 1;
            io.htime = (io.htime & 0xFF) | ((val & 1) << 8);
            io.htime = (io.htime + 1) << 2;
            return;
        case 0x4209: // VTIME lo
            io.vtime = (io.vtime & 0x100) | val;
            return;
        case 0x420A: // VTIME hi
            io.vtime = (io.vtime & 0xFF) | ((val & 1) << 8);
            return;
        case 0x420B:
            for (u32 n = 0; n < 8; n++) {
                R5A22_DMA_CHANNEL *ch = &dma.channels[n];
                ch->dma_enable = (val >> n) & 1;
                //if (dma.channels[n].dma_enable)
            }
            if (val != 0) {
                if (dma.sched.still)
                    scheduler_delete_if_exist(&snes->scheduler, dma.sched.id);
                dma.sched.id = scheduler_add_next(&snes->scheduler, 0, snes, &dma_pending_setup, &dma.sched.still);
            }
            return;
        case 0x420C:
            dma.hdma_enabled = val ? 1 : 0;
            for (u32 n = 0; n < 8; n++) {
                dma.channels[n].hdma_enable = (val >> n) & 1;
            }
            //if (val !== 0) console.log('HDMA CHANNEL WRITE', val, dma.channels[7]);
            return;
        case 0x420D:
            ROMspeed = (val & 1) ? 6 : 8;
            return;

    }

    printf("\nR5A22 MISS WRITE TO %04x %02x", addr, val);
}

void R5A22_hblank(SNES *snes, u32 which)
{
    // evaluate hblank dma
    if (which) {

    }
}

u32 R5A22_reg_read(SNES *snes, u32 addr, u32 old, u32 has_effect, SNES_memmap_block *bl)
{
    addr &= 0xFFFF;
    if ((addr >= 0x4300) && (addr <= 0x43FF)) return dma_reg_read(snes, addr, old, has_effect );
    R5A22 *this = &snes->r5a22;
    u32 val;

    switch(addr) {
        case 0x4000:
            return old;
        case 0x4016: // controller 1 data
            return SNES_controllerport_data(snes, 0);
        case 0x4017: // controller 2 data
            return SNES_controllerport_data(snes, 1);
        case 0x420C:
            return old;
        case 0x4210: // NMI/version read
            val = old & 0x70;
            val |= status.nmi_flag << 7;
            if (has_effect) status.nmi_flag = 0;
            val |= snes->clock.rev;
            return val;
        case 0x4211:
            val = old & 0x7F;
            val |= status.irq_flag << 7;
            if (has_effect) status.irq_flag = 0;
            return val;
        case 0x4212:
            val = old & 0x3E;
            val |= io.auto_joypad_poll && status.auto_joypad.counter < 33;
            val |= snes->clock.ppu.hblank_active << 6;
            val |= snes->clock.ppu.vblank_active << 7;
            return val;
        case 0x4213: // JOYSER1
            return io.pio;
        case 0x4214: // Hardware multiplier stuff
            return io.rddiv & 0xFF;
        case 0x4215:
            return (io.rddiv >> 8) & 0xFF;
        case 0x4216:
            return io.rdmpy & 0xFF;
        case 0x4217:
            return (io.rdmpy >> 8) & 0xFF;
        case 0x4218:
            return io.joy1 & 0xFF;
        case 0x4219:
            return ((io.joy1) >> 8) & 0xFF;
        case 0x421A:
            return io.joy2 & 0xFF;
        case 0x421B:
            return ((io.joy2) >> 8) & 0xFF;
        case 0x421C:
            return io.joy3 & 0xFF;
        case 0x421D:
            return ((io.joy3) >> 8) & 0xFF;
        case 0x421E:
            return io.joy4 & 0xFF;
        case 0x421F:
            return ((io.joy4) >> 8) & 0xFF;
    }
    printf("\nR5A22 MISS READ TO %06x", addr);
    dbg_break("R5A22 MISS READ", snes->clock.master_cycle_count);
    return 0;
}

void R5A22_setup_tracing(R5A22* this, jsm_debug_read_trace *strct)
{
    WDC65816_setup_tracing(&cpu, strct);
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

static void cycle_alu(SNES *snes)
{
    R5A22 *this = &snes->r5a22;
    if (!alu.mpyctr  && !alu.divctr) return;
    if (alu.mpyctr) {
        alu.mpyctr--;
        if (io.rddiv & 1) io.rdmpy += alu.shift;
        io.rddiv >>= 1;
        alu.shift <<= 1;
    }

    if (alu.divctr) {
        alu.divctr--;
        io.rddiv <<= 1;
        alu.shift >>= 1;
        if (io.rdmpy >= alu.shift) {
            io.rdmpy -= alu.shift;
            io.rddiv |= 1;
        }
    }
}

static void cycle_cpu(SNES *snes)
{
    R5A22 *this = &snes->r5a22;
    WDC65816_cycle(&cpu);
    u32 cpu_addr = (cpu.pins.BA << 16) | cpu.pins.Addr;
    snes->clock.cpu.divider = cpu.pins.PDV ? mem_timing(cpu_addr, ROMspeed) : 6;
    if (cpu.pins.PDV) { // Read/write. THIS ONLY WORKS FOR PDV MODE not expanded pins
        if (cpu.pins.RW) { // Write
            //if (((cpu_addr & 0xFFFF) >= 0x2140) && ((cpu_addr & 0xFFFF) < 0x2144)) {
             //   dbgloglog(snes, SNES_CAT_WDC_WRITE, DBGLS_INFO, "%06x   (write MAIL%d) %02x", cpu_addr, cpu_addr & 3, cpu.pins.D);
            //}
            //else {
                dbgloglog(snes, SNES_CAT_WDC_WRITE, DBGLS_INFO, "%06x   (write) %02x", cpu_addr, cpu.pins.D);
            //}
            SNES_wdc65816_write(snes, cpu_addr, cpu.pins.D);
        }
        else { // Read
            cpu.pins.D = SNES_wdc65816_read(snes, cpu_addr, cpu.pins.D, 1);
            //if (((cpu_addr & 0xFFFF) >= 0x2140) && ((cpu_addr & 0xFFFF) < 0x2144)) {
            //    dbgloglog(snes, SNES_CAT_WDC_READ, DBGLS_INFO, "%06x   (read MAIL%d) %02x", cpu_addr, cpu_addr & 3, cpu.pins.D);
            //}
            //else {
                dbgloglog(snes, SNES_CAT_WDC_READ, DBGLS_INFO, "%06x   (read) %02x", cpu_addr, cpu.pins.D);
            //}
        }
    }
}

static inline u32 validA(u32 addr)
{
    if ((addr & 0x40FF00) == 0x2100) return 0;
    if ((addr & 0x40FE00) == 0x4000) return 0;
    if ((addr & 0x40FFE0) == 0x4200) return 0;
    return (addr & 0x40FF80) != 0x4300;
}

static inline u32 dma_readA(SNES *snes, u32 addr) {
    //scheduler_from_event_adjust_master_clock(&snes->scheduler, 8);
    return validA(addr) ? SNES_wdc65816_read(snes, addr, 0, 1) : 0;
}

static inline u32 dma_readB(SNES *snes, u32 addr, u32 valid) {
    //scheduler_from_event_adjust_master_clock(&snes->scheduler, 8);
    return valid ? SNES_wdc65816_read(snes, 0x2100 | addr, 0, 1) : 0;
}

static inline void dma_writeA(SNES *snes, u32 addr, u32 val) {
    if (validA(addr)) {
        SNES_wdc65816_write(snes, addr, val);
    }
}

static inline void dma_writeB(SNES *snes, u32 addr, u32 val, u32 valid)
{
    if (valid) SNES_wdc65816_write(snes, 0x2100 | addr, val);
}

void R5A22_DMA_CHANNEL::dma_transfer(u32 addrA, u32 index, u32 hdma_mode)
{
    u32 addrB = ch->target_address;
    switch(transfer_mode) {
        case 1:
        case 5:
            addrB += (index & 1);
            break;
        case 3:
        case 7:
            addrB += (index >> 1) & 1;
            break;
        case 4:
            addrB += index;
            break;
    }
    u32 valid = addrB != 0x80 || ((addrA & 0xFE0000) != 0x7E0000 && (addrA & 0x40E000) != 0x0000);
    u32 data;
    if (direction == 0) {
        data = dma_readA(snes, addrA);
        if (!hdma_mode) {
            dbgloglog(snes, SNES_CAT_DMA_WRITE, DBGLS_INFO,
                      "DMA%d A->B addrA:%06x addrB:%04x val:%02X valid:%d tsize:%d index:%d vram_addr:%04x", num,
                      addrA, 0x2100 | addrB, data, valid, transfer_size, index, snes->ppu.io.vram.addr);
        }
        else {
            dbgloglog(snes, SNES_CAT_HDMA_WRITE, DBGLS_INFO,
                      "HDMA%d A->B addrA:%06x  addrB:%04x  val:%02X  valid:%d  tsize:%d  index:%d  line_ctr:%02x  y:%d", num,
                      addrA, 0x2100 | addrB, data, valid, transfer_size, index, line_counter, snes->clock.ppu.y);
        }
        dma_writeB(snes, addrB, data, valid);
        //printf("\nDMA%d from %04x to 21%02x len:%d", num, addrA, addrB, transfer_size);
    }
    else {
        data = dma_readB(snes, addrB, valid);
        if (!hdma_mode) {
            dbgloglog(snes, SNES_CAT_DMA_WRITE, DBGLS_INFO,
                      "DMA%d B->A addrA:%06x addrB:%04x val:%02X tsize:%d index:%d", num, addrA, 0x2100 | addrB,
                      data, transfer_size, index);
        }
        else {
            dbgloglog(snes, SNES_CAT_HDMA_WRITE, DBGLS_INFO,
                      "HDMA%d B->A addrA:%06x  addrB:%04x  val:%02X  tsize:%d  index:%d  line_ctr:%02x  y:%d", num, addrA, 0x2100 | addrB,
                      data, transfer_size, index, line_counter, snes->clock.ppu.y);

        }
        dbgloglog(snes, SNES_CAT_DMA_WRITE, DBGLS_INFO, "DMA%d B->A addrA:%06x addrB:%04x val:%02X tsize:%d index:%d", num, addrA, 0x2100 | addrB, data, transfer_size, index);
        dma_writeA(snes, addrA, data);
        //printf("\nDMA%d from 21%02x to %04x", num, addrB, addrA);
    }
}

static void hdma_transfer_ch(SNES *snes, R5A22_DMA_CHANNEL *ch)
{
    static const int HDMA_LENGTHS[8] = {1, 2, 2, 4, 4, 4, 2, 4};
    ch->dma_enable = 0;
    if (!ch->hdma_do_transfer) return;
    for (u32 index = 0; index < HDMA_LENGTHS[ch->transfer_mode]; index++) {
        if (dbg.do_break) return;
        u32 addr;
        if (ch->indirect) {
            addr = (ch->indirect_bank << 16) | ch->indirect_address;
            ch->indirect_address = (ch->indirect_address + 1) & 0xFFFF;
        }
        else {
            addr = (ch->source_bank << 16) | ch->source_address;
            ch->source_address = (ch->source_address + 1) & 0xFFFF;
        }
        dma_transfer(snes, ch, addr, index, 1);
        snes->clock.master_cycle_count += 8;
    }
}

static void hdma_advance_ch(SNES *snes, R5A22_DMA_CHANNEL *ch)
{
    if (!(ch->hdma_enable && !ch->hdma_completed)) return;
    ch->line_counter--;
    ch->hdma_do_transfer = (ch->line_counter & 0x80) >> 7;
    ch->hdma_reload_ch();
}

static u32 dma_run(SNES *snes)
{
    R5A22 *this = &snes->r5a22;
    u32 any_going = 0;
    for (u32 n = 0; n < 8; n++) {
        R5A22_DMA_CHANNEL *ch = &dma.channels[n];
        if (dbg.do_break) return any_going;
        if (ch->hdma_enable && !ch->hdma_completed) {
            hdma_transfer_ch(snes, ch);
            hdma_advance_ch(snes, ch);
        }
        else if (ch->dma_enable) {
            dma_run_ch(snes, ch);
            any_going++;
            break;
        }
    }
    if (!any_going) status.dma_running = 0;
    return 0;
}

void R5A22_cycle(void *ptr, u64 key, u64 clock, u32 jitter)
{
    SNES *snes = (SNES *)ptr;
    R5A22 *this = &snes->r5a22;
    //u64 cur = clock - jitter;
    //scheduler_only_add_abs(&snes->scheduler, cur + snes->clock.cpu.divider, 0, snes, &R5A22_cycle, NULL);
    if (status.dma_running) {
        //u64 before = snes->clock.master_cycle_count;
        dma_run(snes);
        //u64 dif = snes->clock.master_cycle_count - before;
        //printf("\nDIF %lld", dif);
        cycle_alu(snes);
    }
    else {
        cycle_cpu(snes);
        cycle_alu(snes);
        snes->clock.master_cycle_count += snes->clock.cpu.divider;
    }

}

void R5A22_schedule_first(SNES *snes)
{
    //snes->r5a22.status.auto_joypad.sch_id = scheduler_only_add_abs(&snes->scheduler, 128, 0, snes, &auto_joypad_edge, &snes->r5a22.status.auto_joypad.still_sched);
}
