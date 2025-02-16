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

u32 PS1_mainbus_read(void *ptr, u32 addr, u32 sz, u32 has_effect)
{
    struct PS1* this = (struct PS1*)ptr;

    addr = deKSEG(addr);
    // 2MB MRAM mirrored 4 times
    if (addr < 0x00800000) {
        return cR[sz](this->mem.MRAM, addr & 0x1FFFFF);
    }
    // 1F800000 1024kb of scratchpad
    if ((addr >= 0x1F800000) && (addr < 0x1F800400)) {
        return cR[sz](this->mem.scratchpad, addr & 0x3FF);
    }
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

    switch(addr) {
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
            return PS1_GPU_get_gpuread(this);
        case 0x1F801814: // GPUSTAT Read GPU Status Register
            return PS1_GPU_get_gpustat(this);
        case 0x1F801C0C: // Voice 0..23 ADSR Current Volume
        case 0x1F801C1C: //
        case 0x1F801C2C:
        case 0x1F801C3C:
        case 0x1F801C4C:
        case 0x1F801C5C:
        case 0x1F801C6C:
        case 0x1F801C7C:
        case 0x1F801C8C:
        case 0x1F801C9C:
        case 0x1F801CAC:
        case 0x1F801CBC:
        case 0x1F801CCC:
        case 0x1F801CDC:
        case 0x1F801CEC:
        case 0x1F801CFC:
        case 0x1F801D0C:
        case 0x1F801D1C:
        case 0x1F801D2C:
        case 0x1F801D3C:
        case 0x1F801D4C:
        case 0x1F801D5C:
        case 0x1F801D6C:
        case 0x1F801D7C: // ..Voice 0..23 ADSR Current Volume
        case 0x1F801D88: // Voice 0..23 Key ON (Start Attack/Decay/Sustain) (W)
        case 0x1F801D8A: // ..
        case 0x1F801D8C: // Voice 0..23 Key OFF (Start Release) (W)
        case 0x1F801D8E: //  ...
            return 0;
        case 0x1F801DAC: // Sound RAM Data Transfer Control
            return 0;
        case 0x1F801DAA: // SPU Control Register
            return 0;
        case 0x1F801DAE: // SPU Status Register (SPUSTAT) (R)
            return 0;
        case 0x1F000084: // PIO
            //console.log('PIO READ!');
            return 0;
    }

    printf("\nUNHANDLED MAINBUS READ sz %d addr %08x", sz, addr);
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
    addr = deKSEG(addr);
    if ((addr < 0x800000) && !this->mem.cache_isolated) {
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
            printf("WRITE POST STATUS! %d", val);
            return;
        case 0x1F801810: // GP0 Send GP0 Commands/Packets (Rendering and VRAM Access)
            PS1_GPU_write_gp0(this, val);
            return;
        case 0x1F801814: // GP1
            PS1_GPU_write_gp1(this, val);
            return;
        case 0x1F801C00: //  Voice 0..23 stuff
        case 0x1F801C02:
        case 0x1F801C04:
        case 0x1F801C06:
        case 0x1F801C08:
        case 0x1F801C0A:
        case 0x1F801C10:
        case 0x1F801C12:
        case 0x1F801C14:
        case 0x1F801C16:
        case 0x1F801C18:
        case 0x1F801C1A:
        case 0x1F801C20:
        case 0x1F801C22:
        case 0x1F801C24:
        case 0x1F801C26:
        case 0x1F801C28:
        case 0x1F801C2A:
        case 0x1F801C30:
        case 0x1F801C32:
        case 0x1F801C34:
        case 0x1F801C36:
        case 0x1F801C38:
        case 0x1F801C3A:
        case 0x1F801C40:
        case 0x1F801C42:
        case 0x1F801C44:
        case 0x1F801C46:
        case 0x1F801C48:
        case 0x1F801C4A:
        case 0x1F801C50:
        case 0x1F801C52:
        case 0x1F801C54:
        case 0x1F801C56:
        case 0x1F801C58:
        case 0x1F801C5A:
        case 0x1F801C60:
        case 0x1F801C62:
        case 0x1F801C64:
        case 0x1F801C66:
        case 0x1F801C68:
        case 0x1F801C6A:
        case 0x1F801C70:
        case 0x1F801C72:
        case 0x1F801C74:
        case 0x1F801C76:
        case 0x1F801C78:
        case 0x1F801C7A:
        case 0x1F801C80:
        case 0x1F801C82:
        case 0x1F801C84:
        case 0x1F801C86:
        case 0x1F801C88:
        case 0x1F801C8A:
        case 0x1F801C90:
        case 0x1F801C92:
        case 0x1F801C94:
        case 0x1F801C96:
        case 0x1F801C98:
        case 0x1F801C9A:
        case 0x1F801CA0:
        case 0x1F801CA2:
        case 0x1F801CA4:
        case 0x1F801CA6:
        case 0x1F801CA8:
        case 0x1F801CAA:
        case 0x1F801CB0:
        case 0x1F801CB2:
        case 0x1F801CB4:
        case 0x1F801CB6:
        case 0x1F801CB8:
        case 0x1F801CBA:
        case 0x1F801CC0:
        case 0x1F801CC2:
        case 0x1F801CC4:
        case 0x1F801CC6:
        case 0x1F801CC8:
        case 0x1F801CCA:
        case 0x1F801CD0:
        case 0x1F801CD2:
        case 0x1F801CD4:
        case 0x1F801CD6:
        case 0x1F801CD8:
        case 0x1F801CDA:
        case 0x1F801CE0:
        case 0x1F801CE2:
        case 0x1F801CE4:
        case 0x1F801CE6:
        case 0x1F801CE8:
        case 0x1F801CEA:
        case 0x1F801CF0:
        case 0x1F801CF2:
        case 0x1F801CF4:
        case 0x1F801CF6:
        case 0x1F801CF8:
        case 0x1F801CFA:
        case 0x1F801D00:
        case 0x1F801D02:
        case 0x1F801D04:
        case 0x1F801D06:
        case 0x1F801D08:
        case 0x1F801D0A:
        case 0x1F801D10:
        case 0x1F801D12:
        case 0x1F801D14:
        case 0x1F801D16:
        case 0x1F801D18:
        case 0x1F801D1A:
        case 0x1F801D20:
        case 0x1F801D22:
        case 0x1F801D24:
        case 0x1F801D26:
        case 0x1F801D28:
        case 0x1F801D2A:
        case 0x1F801D30:
        case 0x1F801D32:
        case 0x1F801D34:
        case 0x1F801D36:
        case 0x1F801D38:
        case 0x1F801D3A:
        case 0x1F801D40:
        case 0x1F801D42:
        case 0x1F801D44:
        case 0x1F801D46:
        case 0x1F801D48:
        case 0x1F801D4A:
        case 0x1F801D50:
        case 0x1F801D52:
        case 0x1F801D54:
        case 0x1F801D56:
        case 0x1F801D58:
        case 0x1F801D5A:
        case 0x1F801D60:
        case 0x1F801D62:
        case 0x1F801D64:
        case 0x1F801D66:
        case 0x1F801D68:
        case 0x1F801D6A:
        case 0x1F801D70:
        case 0x1F801D72:
        case 0x1F801D74:
        case 0x1F801D76:
        case 0x1F801D78:
        case 0x1F801D7A: // voice stuff
        case 0x1F801D88: // Voice 0..23 Key ON (Start Attack/Decay/Sustain) (W)
        case 0x1F801D8A: // ..
        case 0x1F801DA2: // Sound RAM Reverb Work Area Start Address
            return;
        case 0x1F801D8C: // Voice 0..23 Key OFF (Start Release) (W)
        case 0x1F801D8E: // ...
        case 0x1F801D90: // Voice 0..23 Channel FM (pitch lfo) mode (R/W)
        case 0x1F801D92: // ..
        case 0x1F801D94: // Voice 0..23 Channel Noise mode (R/W)
        case 0x1F801D96: // ..
        case 0x1F801D98: // Voice 0..23 Channel Reverb mode (R/W)
        case 0x1F801D9A: // ..
        case 0x1F801DA6: // Sound RAM Data Transfer Address
        case 0x1F801DA8: // Sound RAM Data Transfer Fifo
        case 0x1F801DAA: // SPU Control Register (SPUCNT)
        case 0x1F801DAC: // Sound RAM Data Transfer Control
        case 0x1F801DB0: // CD volume L
        case 0x1F801DB2: // CD volume R
        case 0x1F801DB4: // Extern volume L
        case 0x1F801DB6: // Extern volume R
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
    return PS1_mainbus_read(ptr, addr, sz, 1);
}
