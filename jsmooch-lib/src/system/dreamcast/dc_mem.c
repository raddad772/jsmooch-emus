#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnreachableCode"
//
//
// Created by Dave on 2/13/2024.
//

#include "assert.h"
#include "stdio.h"
#include "dc_mem.h"
#include "holly.h"
#include "gdrom.h"


u64 DCread_flash(struct DC* this, u32 addr, u32* success, u32 bits);
void G1_write(struct DC* this, u32 reg, u32 val, u32 bits, u32* success);

#define B32(b31_b28, b27_24,b23_20,b19_16,b15_12,b11_8,b7_4,b3_0) ( \
  ((0b##b31_b28) << 28) | ((0b##b27_24) << 24) | \
  ((0b##b23_20) << 20) | ((0b##b19_16) << 16) | \
  ((0b##b15_12) << 12) | ((0b##b11_8) << 8) | \
  ((0b##b7_4) << 4) | (0b##b3_0))


#define THIS struct DC* this = (struct DC*)ptr

static u32 dcms(enum DC_MEM_SIZE sz)
{
    switch(sz) {
        case DC8:
            return 8;
        case DC16:
            return 16;
        case DC32:
            return 32;
        case DC64:
            return 64;
    }
}

static void gdrom_dma_start(struct DC* this)
{
    if (this->gdrom.SB_GDST) printf("\nGDROM DMA REQUEST!");
}

static void sb_dma_start(struct DC* this, i32 channel, u32 reg_addr)
{
    printf("\nWAIT WHAT? DMA START %d %d", channel, this->io.SB_C2DST);
}

 u64 read_empty(struct DC* this, u32 addr, enum DC_MEM_SIZE sz, u32 *success)
 {
    *success = 0;

    printf("\nRead empty addr %08x full:%08X", (addr & 0x1FFFFFFF), addr);
    fflush(stdout);
    return 0;
 }

 void write_empty(struct DC* this, u32 addr, u64 val, enum DC_MEM_SIZE sz, u32* success)
 {
     printf("\nWrite empty addr %08x full:%08X val:%08llx", (addr & 0x1FFFFFFF), addr, val);
     fflush(stdout);
     *success = 0;
 }

 u64 cR8(void *ptr, u32 addr) {
    return ((u8 *)ptr)[addr];
 }

 u64 cR16(void *ptr, u32 addr) {
    return *(u16*)(((u8*)ptr)+addr);
}

u64 cR32(void *ptr, u32 addr) {
    return *(u32*)(((u8*)ptr)+addr);
}

u64 cR64(void *ptr, u32 addr) {
    return *(u64*)(((u8*)ptr)+addr);
}

void cW8(void *ptr, u32 addr, u64 val) {
    *(((u8*)ptr)+addr) = val;
}

void cW16(void *ptr, u32 addr, u64 val) {
    *(u16 *)(((u8*)ptr)+addr) = val;
}

void cW32(void *ptr, u32 addr, u64 val) {
    *(u32 *)(((u8*)ptr)+addr) = val;
}

void cW64(void *ptr, u32 addr, u64 val) {
    *(u64 *)(((u8*)ptr)+addr) = val;
}

static u64 (*cR[9])(void *, u32) = {
        NULL,
        &cR8,
        &cR16,
        NULL,
        &cR32,
        NULL,
        NULL,
        NULL,
        &cR64
};

static void (*cW[9])(void *, u32, u64) = {
        NULL,
        &cW8,
        &cW16,
        NULL,
        &cW32,
        NULL,
        NULL,
        NULL,
        &cW64
};


u64 read_area0(struct DC* this, u32 addr, enum DC_MEM_SIZE sz, u32* success)
 {
     u32 full_addr = addr;
     addr &= 0x1FFFFFFF; // only 29 bits of real addresses I guess
     if (addr <= 0x1FFFFF)
         return cR[sz](this->BIOS.ptr, addr & 0x1FFFFF);
     if ((addr >= 0x200000 ) && (addr <= 0x21FFFF))
         return DCread_flash(this, addr, success, 4);

     if ((addr >= 0x005F8000) && (addr <= 0x005FFFFF))
         return holly_read(this, addr, success);

     // handle Operand Cache access
     if ((full_addr >= 0x7C000000) && (full_addr <= 0x7FFFFFFF)) {
         if (this->sh4.regs.CCR.OIX == 0)
             return cR[sz](this->OC, ((addr & 0x2000) >> 1) | (addr & 0xFFF));
         else
             return cR[sz](this->OC, ((addr & 0x02000000) >> 13) | (addr & 0xFFF));
     }

     switch(full_addr) {
#include "generated/area0_reads.c"
         case 0x005F6900: // Interrupt status register SB_ISTNRM
             // Clear anything that a 1 is written to in bits 21 to 0
#ifdef LYCODER
             return 8; // stub for IP.BIN to bypass vblank wait
#else
             return this->io.SB_ISTNRM;
#endif
     }

     *success = 0;
     return 0;
 }


 static void maple_write(struct DC* this, u32 addr, u64 val, enum DC_MEM_SIZE sz, u32* success)
{
    addr &= 0x1FFFFFFF;
    switch(addr) {
#include "generated/maple_writes.c"
    }
    printf("\nYEAH GOT HERE SORRY DAWG");
    *success = 0;
}

 void write_area0(struct DC* this, u32 addr, u64 val, enum DC_MEM_SIZE sz, u32* success)
 {
    u32 full_addr = addr;
    addr &= 0x1FFFFFFF;

    if ((full_addr >= 0x7C000000) && (full_addr <= 0x7FFFFFFF)) {
         if (this->sh4.regs.CCR.OIX == 0)
             cW[sz](this->OC, ((full_addr & 0x2000) >> 1) | (full_addr & 0xFFF), val);
         else
             cW[sz](this->OC, ((full_addr & 0x02000000) >> 13) | (full_addr & 0xFFF), val);
         return;
     }

     addr &= 0x1FFFFFFF;
     if ((addr >= 0x005F7000) && (addr <= 0x005F70FF)) { // General G1 commands
         G1_write(this, full_addr, val, sz, success);
         return;
     }

     if ((addr >= 0x005F7400) && (addr <= 0x005F74FF)) { // GDROM commands
         G1_write(this, full_addr, val, sz, success);
         return;
     }

     if ((addr >= 0x005F6C00) && (addr <= 0x005F6CFF)) { // Maple commands!
         maple_write(this, full_addr, val, sz, success);
         return;
     }

     if ((addr >= 0x005F8000) && (addr <= 0x005FFFFF)) {
         holly_write(this, addr, val, success);
         return;
     }

     switch(addr) {
#include "generated/area0_writes.c"
         case 0x005F6900: // Interrupt status register SB_ISTNRM
             // Clear anything that a 1 is written to in bits 21 to 0
             printf("\nCLEAR ISTNRM %08llx", val);
             this->io.SB_ISTNRM ^= this->io.SB_ISTNRM & val & 0x3FFFFF;
             return;
     }

     *success = 0;
 }

u64 read_area1(struct DC* this, u32 addr, enum DC_MEM_SIZE sz, u32* success)
{
    u32 full_addr = addr;
    addr &= 0x1FFFFFFF; // only 29 bits of real addresses I guess

    if ((addr >= 0x05000000) && (addr <= 0x05800000)) // VRAM access
        return cR[sz](this->VRAM, addr & 0x057FFFFF);

    *success = 0;
    return 0;
}

void write_area1(struct DC* this, u32 addr, u64 val, enum DC_MEM_SIZE sz, u32* success)
{
    addr &= 0x1FFFFFFF; // only 29 bits of real addresses I guess
    if ((addr >= 0x05000000) && (addr <= 0x05800000)) { // VRAM 32bit access
        cW[sz](this->VRAM, addr & 0x057FFFFF, val);
        return;
    }

    *success = 0;
}

u64 read_area3(struct DC* this, u32 addr, enum DC_MEM_SIZE sz, u32* success)
{
    addr &= 0x1FFFFFFF;
    if (((addr >= 0x0C000000) && (addr <= 0x0DFFFFFF)))// || ((addr >= 0x0E000000) && (addr <= 0x0EFFFFFF)))
        return cR[sz](this->RAM, addr & 0xFFFFFF);

    *success = 0;
    return 0;
}

void write_area3(struct DC* this, u32 addr, u64 val, enum DC_MEM_SIZE sz, u32* success)
{
    addr &= 0x1FFFFFFF;
    if (((addr >= 0x0C000000) && (addr <= 0x0DFFFFFF))) {//|| ((addr >= 0x0E000000) && (addr <= 0x0EFFFFFF))) {
        cW[sz](this->RAM, addr & 0xFFFFFF, val);
        return;
    }

    *success = 0;
}

u64 read_area4(struct DC* this, u32 addr, enum DC_MEM_SIZE sz, u32* success)
{
    assert(1==0);
}

void write_area4(struct DC* this, u32 addr, u64 val, enum DC_MEM_SIZE sz, u32* success)
{
    assert(1==0);
}

u64 read_P4(struct DC* this, u32 addr, enum DC_MEM_SIZE sz, u32* success)
{
    addr |= 0xF0000000;
    switch (addr | 0xE0000000) {
#include "generated/p4_reads.c"
        case 0xFF000024: //
            return this->sh4.regs.EXPEVT;
        case 0xFF800030: { // PDTRA
            assert(sz==2);
            // PDTRA from Bus Control
            // Note: I got it from Deecy...
            // Note: I have absolutely no idea what's going on here.
            //       This is directly taken from Flycast, which already got it from Chankast.
            //       This is needed for the bios to work properly, without it, it will
            //       go to sleep mode with all interrupts disabled early on.
            u32 tpctra = this->io.PCTRA;
            u32 tpdtra = this->io.PDTRA;

            u16 tfinal = 0;
            if ((tpctra & 0xf) == 0x8) {
                tfinal = 3;
            } else if ((tpctra & 0xf) == 0xB) {
                tfinal = 3;
            } else {
                tfinal = 0;
            }

            if (((tpctra & 0xf) == 0xB) && ((tpdtra & 0xf) == 2)) {
                tfinal = 0;
            } else if (((tpctra & 0xf) == 0xC) && ((tpdtra & 0xf) == 2)) {
                tfinal = 3;
            }

            tfinal |= 0; // 0=VGA, 2=RGB, 3=composite //@intFromEnum(self._dc.?.cable_type) << 8;

            return tfinal;
        }
        case 0xFF800028: // RFCR
            // doc a little unclear on this
            //return this->io.RFCR;
            return 0x0011; // to pass BIOS check
        case  0xFFD80004:  // TSTR
            return 0;
    }

    *success = 0;
    return 0;
}

void write_P4(struct DC* this, u32 addr, u64 val, enum DC_MEM_SIZE sz, u32* success)
{
    u32 full_addr = addr;
    u32 up_addr = addr | 0xE0000000;
    addr &= 0x1FFFFFFF;
    if ((full_addr >= 0xE0000000) && (full_addr <= 0xE3FFFFFF)) { // store queue write
        cW[sz](this->sh4.SQ[(addr >> 5) & 1], addr & 0x1C, val);
        return;
    }

    switch(up_addr) {
#include "generated/p4_writes.c"
        case 0xFF000020: // EXPEVT
            this->sh4.regs.EXPEVT = val & 0xFFF;
            return;
        case 0xFF000024: // TRAPA
            this->sh4.regs.TRAPA = val & 0xFF;
            return;
        case 0xFF000028: // INTEVT
            this->sh4.regs.INTEVT = val & 0xFFFF;
            return;
        case 0xFF800030: // PDTRA
            return;
        case 0xFF800028: // RFCR
            // doc a little unclear on this
            this->io.RFCR = 0b1010010000000000 | (val & 0x1FF);
            return;
        case 0xFF800000: // BSCR
            this->io.BSCR = val;
            printf("\nWRITE TO BSCR!"); // ignore?
            return;
        case 0xFF000038: // QACR0 for store queues
            this->sh4.regs.QACR0 = val;
            return;
        case 0xFF00003C: // QACR1 for store queues
            this->sh4.regs.QACR1 = val;
            return;
        case 0xff00001c: // Cache control CCR
            this->sh4.regs.CCR.IIX = (val >> 15) & 1;
            this->sh4.regs.CCR.ICI = (val >> 11) & 1;
            this->sh4.regs.CCR.ICE = (val >> 8) & 1;
            this->sh4.regs.CCR.OIX = (val >> 7) & 1;
            this->sh4.regs.CCR.ORA = (val >> 5) & 1;
            this->sh4.regs.CCR.OCI = (val >> 3) & 1;
            this->sh4.regs.CCR.CB = (val >> 2) & 1;
            this->sh4.regs.CCR.WT = (val >> 1) & 1;
            this->sh4.regs.CCR.OCE = val & 1;
            printf("\nOIX:%d ORA:%d OCE:%d", this->sh4.regs.CCR.OIX, this->sh4.regs.CCR.ORA, this->sh4.regs.CCR.OCE);
            //fflush(stdout);
            return;
        case 0xffd80000: // TOCR timer output control register
            assert(sz==1);
            this->io.TOCR = 0;
            return;
        case 0xffd80004: // TSTR
            assert(sz==1);
            this->io.TSTR = 0;
            return;
    }
    *success = 0;
}



void DC_mem_init(struct DC* this)
{
    for (u32 i = 0; i < 0x40; i++) {
        this->mem.read[i] = &read_empty;
        this->mem.write[i] = &write_empty;
    }

#define MAP(addr, rdfunc, wrfunc) this->mem.read[(addr)>>2] = &(rdfunc); this->mem.write[(addr)>>2] = &(wrfunc)
    // All of P0. P1, P2, P3 should mirror the lower half of P0?
    u32 areas[4] = { 0x00, 0x80, 0xA0, 0xC0 };
    for (u32 i = 0; i < 4; i++) {
        u32 area = areas[i];
        MAP(area + 0x00, read_area0, write_area0);
        MAP(area + 0x04, read_area1, write_area1);
        //MAP(0x08, read_area2, write_area2);
        MAP(area + 0x0C, read_area3, write_area3);
        MAP(area + 0x10, read_area4, write_area4);
        //MAP(0x14, read_area5, write_area5);
        //MAP(0x18, read_area6, write_area6);
        MAP(area + 0x1C, read_P4, write_P4);
    }

    for (u32 addr = 0xE0; addr < 0x100; addr += 4) {
        MAP(addr, read_P4, write_P4);
    }
#undef MAP
}



u64 VMASK[9] = {
        0,
        0xFF, // 1 = 8-bit
        0xFFFF, // 2 = 16-bit
        0,
        0xFFFFFFFF, // 4 = 32-bit
        0,
        0,
        0,
        0xFFFFFFFFFFFFFFFF // 8 = 64-bit?
};

#define RFO addr, ret
#define WFO addr, val
static char RFORM[9][100] = {
        "",
        "\n" DBGC_READ " read8    (%08x) %02x" DBGC_RST,
        "\n" DBGC_READ " read16   (%08x) %04x" DBGC_RST,
        "",
        "\n" DBGC_READ " read32   (%08x) %08x" DBGC_RST,
        "",
        "",
        "",
        "\n" DBGC_READ " read64   (%08x) %16llu" DBGC_RST
};

static char WFORM[9][100] = {
        "",
        "\n" DBGC_WRITE " write8   (%08x) %02llx" DBGC_RST, // 1 = 8-bit
        "\n" DBGC_WRITE " write16  (%08x) %04llx" DBGC_RST, // 2 = 16-bit
        "",
        "\n" DBGC_WRITE " write32  (%08x) %08llx" DBGC_RST, // 4 = 32-bit
        "",
        "",
        "",
        "\n" DBGC_WRITE " write64  (%08x) %016llx" DBGC_RST // 8 = 64-bit
};


void DCwrite(void *ptr, u32 addr, u64 val, u32 sz) {
    THIS;
    u32 success = 1;
    val &= VMASK[sz];

#ifdef SH4_DBG_SUPPORT
    if (dbg.trace_on) {
        dbg_printf(WFORM[sz], WFO);
    }
#endif
#ifdef DO_LAST_TRACES
    dbg_LT_printf(WFORM[sz], WFO);
    dbg_LT_endline();
#endif
    this->mem.write[addr >> 26](this, addr, val, sz, &success);

    if (!success) {
        //printf("\nwrite%d unknown addr %08x %08x val %02llu cycle:%llu", dcms(sz), addr & 0x1FFFFFFF, addr, val,
        //       this->sh4.trace_cycles);
        if (addr >= 0xFF000000)
            printf("\n0x%08X: UKN%08X\nu32\naccess_32, rw\n", addr, addr);
        else
            printf("\n0x%08X: \nu32\naccess_32, rw\n", addr & 0x1FFFFFFF);
        fflush(stdout);
        dbg.var++;
        if (dbg.var > 15) {
            dbg_break();
        }

    }
}

u64 DCread(void *ptr, u32 addr, u32 sz, bool is_ins_fetch)
{
    THIS;
    u32 success = 1;
    u64 ret = this->mem.read[addr >> 26](this, addr, sz, &success) & VMASK[sz];
#ifdef SH4_DBG_SUPPORT
    if (!is_ins_fetch) {
        if (dbg.trace_on) {
            dbg_printf(RFORM[sz], RFO);
        }
    }
#endif
#ifdef DO_LAST_TRACES
    if (!is_ins_fetch) {
        dbg_LT_printf(RFORM[sz], RFO);
        dbg_LT_endline();
    }
#endif

    if (!success) {
        printf("\nunknown read%d from %08x", dcms(sz), addr);
        return 0;
    }
    return ret;
}


u64 DCread_flash(struct DC* this, u32 addr, u32* success, u32 bits)
{
    printf("\nUNSUPPORTED FLASH READ JUST YET");
    *success = 0;
    switch(addr) {

    }
    return *(u32 *)((u8*)this->flash.ptr + (addr & 0x1FFFF));
}

u32 DCfetch_ins(void *ptr, u32 addr)
{
    return DCread(ptr, addr, 2, true);
}

u64 DCread_noins(void *ptr, u32 addr, u32 sz)
{
    return DCread(ptr, addr, sz, false);
}

void G1_write(struct DC* this, u32 addr, u32 val, u32 bits, u32* success)
{
    addr &= 0x1FFFFFFF;
    switch(addr) {
#include "generated/gdrom_writes.c"
        case 0x005F7480: { // SB_G1RRC write-only timing for system ROM accesses
            return;
        }
        case 0x005F74E4: { // Secret GDROM unlock register!
            return;
        }
    }
    *success = 0;
    printf("\nUnhandled G1 reg write %02x val %04x bits %d", addr, val, bits);
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
#pragma clang diagnostic pop