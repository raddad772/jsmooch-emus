//
// Created by Dave on 2/13/2024.
//

#include "stdio.h"
#include "dc_mem.h"

#define THIS struct DC* this = (struct DC*)ptr

u32 DCread8(void *ptr, u32 addr)
{
    THIS;
    printf("\nread8 unknown addr %08x", addr);
    fflush(stdout);
    return 0;
}

u32 DCread16(void *ptr, u32 addr)
{
    THIS;
    addr &= 0x1FFFFFFF; // only 29 bits of real addresses I guess
    i64 ret = -1;
    if (addr < 0x1FFFFF)
        ret = *(u16 *)(((u8 *)this->BIOS.ptr+addr));
    if ((addr >= 0x0C000000) && (addr <= 0x0CFFFFFF))
        ret = *(u16 *)(this->RAM+(addr - 0xC000000));

    if (ret < 0) {
        printf("\nread16 unknown addr %08x", addr);
        fflush(stdout);
        return 0;
    }
    //printf("\nR16 A:%08x V:%04x", (u32)addr, (u32)ret);
    return (u32)ret;
}

u32 DCread32(void *ptr, u32 addr) {
    THIS;
    i64 ret = -1;
    addr &= 0x1FFFFFFF; // only 29 bits of real addresses I guess
    if (addr < 0x1FFFFF)
        ret = *(u32 *) (((u8 *) this->BIOS.ptr + addr));
    if ((addr >= 0x0C000000) && (addr <= 0x0CFFFFFF))
        ret = *(u32 *) (this->RAM + (addr - 0xC000000));

    if (ret < 0) {
        printf("\nread32 unknown addr %08x", addr);
        fflush(stdout);
        return 0;
    }
    dbg_printf("\nRd32 A:%08X V:%08x", (u32) addr, (u32) ret);
    fflush(stdout);
    return ret;
}

void DCwrite8(void *ptr, u32 addr, u32 val)
{
    THIS;
    if ((addr >= 0x0C000000) && (addr < 0x0D000000)) {
        this->RAM[addr - 0xC000000] = (u8)val;
        return;
    }
    dbg_printf("\nwrite8 unknown addr %08x val %02x cycle:%llu", addr, val, this->sh4.trace_cycles);
}

void DCwrite16(void *ptr, u32 addr, u32 val)
{
    THIS;
    printf("\nwrite16 unknown addr %08x val %02x", addr, val);
    fflush(stdout);
}

void DCwrite32(void *ptr, u32 addr, u32 val)
{
    THIS;
    addr &= 0x1FFFFFFF;
    if ((addr >= 0x04000000) && (addr <= 0x04FFFFFF)) { // tex. mem 64-bit access

    }
    if ((addr >= 0x05000000) && (addr <= 0x05800000)) { // VRAM 32bit access
        *((u32 *)(&this->VRAM[addr - 0x05000000])) = val;
        dbg_printf("\nVRAM WRITE A:%08x R:%06x V:%08x", addr, addr - 0x05000000, val);
        return;
    }
    if ((addr >= 0x0C000000) && (addr < 0x0D000000)) {
        *((u32 *) (&this->RAM[addr - 0xC000000])) = val;
        return;
    }

    printf("\nwrite32 unknown addr %08x val %02x", addr, val);
    fflush(stdout);
}

/*
 * it's a bit funky
0x04000000 - 8MB VRAM 64bit mode access

1
[8:13 AM]
0x04800000 - 2nd half of VRAM, doesnt exists in retail Dreamcast, write does nothing read iirc as FF
[8:13 AM]
0x05000000 - 8MB VRAM 32bit mode access
0x05800000 - 2nd half of VRAM, doesnt exists in retail Dreamcast, write does nothing read iirc as FF
[8:14 AM]
area 0x06000000 - 0x07ffffff - mirror of above


 0x8c000_0000 is syscalls.bin (optional)
0x8c000_8000 is ip.bin
0x8c001_0000 is main game executable (1stread.bin usually)
[11:02 AM]
the address space has some mirrors, so not flat 4 gb
[11:02 AM]
the mmu isn't needed though


 */