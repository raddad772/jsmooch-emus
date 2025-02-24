//
// Created by . on 2/11/25.
//

#include <stdio.h>

#include "ps1_bus.h"
#include "ps1_dma.h"
#include "helpers/multisize_memaccess.c"

#define deKSEG(addr) ((addr) & 0x1FFFFFFF)

void PS1_bus_init(struct PS1 *this)
{
    this->dma.control = 0x7654321;
    for (u32 i = 0; i < 7; i++) {
        this->dma.channels[i].num = i;
        this->dma.channels[i].step = PS1D_increment;
        this->dma.channels[i].sync = PS1D_manual;
        this->dma.channels[i].direction = PS1D_to_ram;
    }
}

void PS1_bus_reset(struct PS1 *this)
{
    this->io.cached_isolated = 0;
}

static const u32 alignmask[5] = { 0, 0xFFFFFFFF, 0xFFFFFFFE, 0, 0xFFFFFFFC };

u32 PS1_mainbus_read(void *ptr, u32 addr, u32 sz, u32 has_effect)
{
    struct PS1* this = (struct PS1*)ptr;

    addr = deKSEG(addr) & alignmask[sz];
    // 2MB MRAM mirrored 4 times
    if (addr < 0x00800000) {
        return cR[sz](this->mem.MRAM, addr & 0x1FFFFF);
    }
    // 1F800000 1024kb of scratchpad
    if ((addr >= 0x1F800000) && (addr < 0x1F800400)) {
        return cR[sz](this->mem.scratchpad, addr & 0x3FF);
    }
    /*if ((addr >= 0x1F800000) && (addr < 0x1F803000)) {
        // TODO: stub: IO area
        return 0;
    }*/
    // 1FC00000h 512kb BIOS
    if ((addr >= 0x1FC00000) && (addr < 0x1FC80000)) {
        return cR[sz](this->mem.BIOS, addr & 0x7FFFF);
    }

    if ((addr >= 0x1F801070) && (addr <= 0x1F801074)) {
        return R3000_read_reg(&this->cpu, addr, sz);
    }

    if ((addr >= 0x1F801080) && (addr <= 0x1F8010FF)) {
        return PS1_DMA_read(this, addr, sz);
    }

    if ((addr >= 0x1F801040) && (addr < 0x1F801060)) {
        static u32 mre = 0;
        if (!mre) {
            mre = 1;
            printf("\nIMPLEMENT MEMORY CARD AND GAMEPAD SHENANIGANS!");
        }
        return 0;
    }

    if ((addr >= 0x1F801C00) && (addr < 0x1F801E00)) {
        return PS1_SPU_read(&this->spu, addr, sz, has_effect);
    }

    switch(addr) {
        case 0x1f801110:
            // expansion area poll
            return 0;
        case 0x00FF1F00: // Invalid addresses?
        case 0x00FF1F04:
        case 0x00FF1F08:
        case 0x00FF1F0C:
        case 0x00FF1F50:
            return 0;
        case 0x1F80101C: // Expansion 2 Delay/size
            return 0x00070777;
        case 0x1F8010A8: // DMA2 GPU thing
        case 0x1F801810: // GP0/GPUREAD
            return PS1_GPU_get_gpuread(&this->gpu);
        case 0x1F801814: // GPUSTAT Read GPU Status Register
            return PS1_GPU_get_gpustat(&this->gpu);
        case 0x1F000084: // PIO
            //console.log('PIO READ!');
            return 0;
    }

    static u32 e = 0;
    printf("\nUNHANDLED MAINBUS READ sz %d addr %08x", sz, addr);
    e++;
    if (e > 10) dbg_break("TOO MANY BAD READ", this->clock.master_cycle_count+this->clock.waitstates);
    switch(sz) {
        case 1:
            return 0xFF;
        case 2:
            return 0xFFFF;
        case 4:
            return 0xFFFFFFFF;
    }
    return 0;
}

void PS1_mainbus_write(void *ptr, u32 addr, u32 sz, u32 val)
{
    struct PS1* this = (struct PS1*)ptr;
    if (this->mem.cache_isolated) return;
    addr = deKSEG(addr) & alignmask[sz];
    /*if (addr == 0) {
        printf("\nWRITE %08x:%08x ON cycle %lld", addr, val, ((struct PS1 *)ptr)->clock.master_cycle_count);
        dbg_break("GHAHA", 0);
    }*/
    if ((addr < 0x00800000) && !this->mem.cache_isolated) {
        cW[sz](this->mem.MRAM, addr & 0x1FFFFF, val);
        return;
    }
    if (((addr >= 0x1F800000) && (addr <= 0x1F800400)) && !this->mem.cache_isolated) {
        cW[sz](this->mem.scratchpad, addr & 0x3FF, val);
        return;
    }

    if ((addr >= 0x1F801070) && (addr <= 0x1F801074)) {
        R3000_write_reg(&this->cpu, addr, sz, val);
        return;
    }

    if ((addr >= 0x1F801080) && (addr <= 0x1F8010FF)) {
        PS1_DMA_write(this, addr, sz, val);
        return;
    }

    if ((addr >= 0x1F801040) && (addr < 0x1F801060)) {
        printf("\nImplement controller BS... %08x", addr);
        return;
        //return this.pad_memcard.CPU_write(addr - 0x1F801040, size, val, this.ps1!);
    }

    if ((addr >= 0x1F801C00) && (addr < 0x1F801E00)) {
        PS1_SPU_write(&this->spu, addr, sz, val);
        return;
    }

    switch(addr) {
        case 0x00FF1F00: // Invalid addresses
        case 0x00FF1F04:
        case 0x00FF1F08:
        case 0x00FF1F0C:
        case 0x00FF1F2C:
        case 0x00FF1F34:
        case 0x00FF1F3C:
        case 0x00FF1F40:
        case 0x00FF1F4C:
        case 0x00FF1F50:
        case 0x00FF1F60:
        case 0x00FF1F64:
        case 0x00FF1F7C:
            return;
        case 0x1F802041: // F802041h 1 PSX: POST (external 7 segment display, indicate BIOS boot status
            //printf("\nWRITE POST STATUS! %d", val);
            return;
        case 0x1F801810: // GP0 Send GP0 Commands/Packets (Rendering and VRAM Access)
            PS1_GPU_write_gp0(&this->gpu, val);
            return;
        case 0x1F801814: // GP1
            PS1_GPU_write_gp1(&this->gpu, val);
            return;
        case 0x1F801000: // Expansion 1 base addr
        case 0x1F801004: // Expansion 2 base addr
        case 0x1F801008: // Expansion 1 delay/size
        case 0x1F80100C: // Expansion 3 delay/size
        case 0x1F801010: // BIOS ROM delay/size
        case 0x1F801014: // SPU_DELAY delay/size
        case 0x1F801018: // CDROM_DELAY delay/size
        case 0x1F80101C: // Expansion 2 delay/size
        case 0x1F801020: // COM_DELAY /size
        case 0x1F801060: // RAM SIZE, 2mb mirrored in first 8mb
        case 0x1F801100: // Timer 0 dotclock
        case 0x1F801104: // ...
        case 0x1F801108: // ...
        case 0x1F801110: // Timer 1 hor. retrace
        case 0x1F801114: // ...
        case 0x1F801118: // ...
        case 0x1F801120: // Timer 2 1/8 system clock
        case 0x1F801124: // ...
        case 0x1F801128: // ...
        case 0x1F801D80: // SPU main vol L
        case 0x1F801D82: // ...R
        case 0x1F801D84: // Reverb output L
        case 0x1F801D86: // ... R
        case 0x1FFE0130: // Cache control
            return;
    }

    printf("\nUNHANDLED MAINBUS WRITE %08x: %08x", addr, val);
}

u32 PS1_mainbus_fetchins(void *ptr, u32 addr, u32 sz)
{
    struct PS1 *this = (struct PS1 *)ptr;
    return PS1_mainbus_read(ptr, addr, sz, 1);
}
