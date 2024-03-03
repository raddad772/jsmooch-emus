//
// Created by Dave on 2/13/2024.
//

#include "stdio.h"
#include "dc_mem.h"
#include "holly.h"
#include "gdrom.h"

#define THIS struct DC* this = (struct DC*)ptr

#define READ16FROM8(arr, addr) (*(u16 *)(arr[addr]))
#define READ32FROM8(arr, addr) (*(u32 *)addr[addr]))
#define WRITE16TO8(arr, addr, val) {*(u16 *)(arr[addr]) = (u16)val; }
#define WRITE32TO8(arr, addr, val) {*(u32 *)(arr[addr]) = val; }

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

static u32 read_empty(struct DC* this, u32 addr, enum DC_MEM_SIZE sz)
{
    u32 maddr = addr & 0x1FFFFFFF;

    printf("\nREAD%d from UNKNOWN addr:%08x %08x", dcms(sz), addr, maddr);
    fflush(stdout);
    return 0;
}

static void write_empty(struct DC* this, u32 addr, u32 val, enum DC_MEM_SIZE sz)
{
    u32 maddr = addr & 0x1FFFFFFF;
    fflush(stdout);

    printf("\nWRITE%d to UNKNOWN addr:%08x %08x %08x", dcms(sz), addr, maddr, val);
    fflush(stdout);
}

u32 DCreadRAM(struct DC* this, u32 addr, enum DC_MEM_SIZE sz)
{
    return 0;
}

void DCwriteRAM(struct DC* this, u32 addr, u32 val, enum DC_MEM_SIZE sz)
{

}

void DC_mem_init(struct DC* this)
{
    for (u32 i = 0; i < 0x20; i++) {
        this->mem.read_map[i] = &read_empty;
        this->mem.write_map[i] = &write_empty;
    }

    this->io.TCOR0 = this->io.TCNT0 = this->io.PCTRA = this->io.PDTRA = 0;
    this->io.SB_LMMODE0 = 0;
    this->io.SB_LMMODE1 = 0;

    // 0C - RAM. EC, CC, 1C, 8C
    //
#define MAP(addr, rdfunc, wrfunc) this->mem.read_map[addr] = &(rdfunc); this->mem.write_map[addr] = &(wrfunc)
    MAP(0x0C, DCreadRAM, DCwriteRAM);
    MAP(0x8C, DCreadRAM, DCwriteRAM);
    MAP(0x8C, DCreadRAM, DCwriteRAM);
}

u32 DCread(struct DC* this, u32 addr, enum DC_MEM_SIZE sz)
{
    return 0;
}

void DCwrite(struct DC* this, u32 addr, u32 val, enum DC_MEM_SIZE sz)
{

}


u32 DCread8(void *ptr, u32 addr)
{
    THIS;
    u32 maddr = addr;
    addr &= 0x1FFFFFFF; // only 29 bits of real addresses I guess
    if (addr <= 0x1FFFFF)
        return ((u8 *)this->BIOS.ptr)[addr];
    if (((addr >= 0x0C000000) && (addr <= 0x0CFFFFFF)) || ((addr >= 0x0E000000) && (addr <= 0x0EFFFFFF)))
        //ret = this->RAM[addr - 0xC000000];
        return this->RAM[addr & 0xFFFFFF];

    // handle Operand Cache access
    if ((maddr >= 0x7C000000) && (maddr <= 0x7FFFFFFF)) {
        if (this->sh4.regs.CCR.OIX == 0)
            return this->OC[((addr & 0x2000) >> 1) | (addr & 0xFFF)];
        else
            return this->OC[((addr & 0x02000000) >> 13) | (addr & 0xFFF)];
    }

    switch(maddr) {
        case  0xFFD80004:  // TSTR
            return 0;
    }
    printf("\nread8 from UNKNOWN addr:%08x %08x", addr, maddr);
    fflush(stdout);
    return 0;
}

u32 DCread16(void *ptr, u32 addr)
{
    THIS;
    u32 maddr = addr;
    addr &= 0x1FFFFFFF; // only 29 bits of real addresses I guess
    i64 ret = -1;
    if (addr < 0x1FFFFF)
        ret = *(u16 *)(((u8 *)this->BIOS.ptr+addr));
    if (((addr >= 0x0C000000) && (addr <= 0x0CFFFFFF)) || ((addr >= 0x0E000000) && (addr <= 0x0EFFFFFF)))
        ret = *(u16 *)(this->RAM+(addr & 0xFFFFFF));

    switch(maddr) {
        case 0xFF800030: // PDTRA from Bus Control
            // Note: I got it from Deecy...
            // Note: I have absolutely no idea what's going on here.
            //       This is directly taken from Flycast, which already got it from Chankast.
            //       This is needed for the bios to work properly, without it, it will
            //       go to sleep mode with all interrupts disabled early on.
            ;u32 tpctra = this->io.PCTRA;
            u32 tpdtra = this->io.PDTRA;

            u16 tfinal = 0;
            if ((tpctra & 0xf) == 0x8) {
                tfinal = 3;
            } else if ((tpctra & 0xf) == 0xB) {
                tfinal = 3;
            } else {
                tfinal = 0;
            }

            if (((tpctra & 0xf) == 0xB) && ( (tpdtra & 0xf) == 2)) {
                    tfinal = 0;
            } else if (((tpctra & 0xf) == 0xC) && ((tpdtra & 0xf) == 2)) {
            tfinal = 3;
            }

            tfinal |= 0; // 0=VGA, 2=RGB, 3=composite //@intFromEnum(self._dc.?.cable_type) << 8;

            return tfinal;
        case 0xFF800028: // RFCR
            // doc a little unclear on this
            return this->io.RFCR;
    }
    // handle Operand Cache access
    if ((maddr >= 0x7C000000) && (maddr <= 0x7FFFFFFF)) {
        if (this->sh4.regs.CCR.OIX == 0)
            return *(u16 *)(&this->OC[((addr & 0x2000) >> 1) | (addr & 0xFFF)]);
        else
            return *(u16 *)(&this->OC[((addr & 0x02000000) >> 13) | (addr & 0xFFF)]);
    }


    if (ret < 0) {
        printf("\nread16 unknown addr %08x %08x", addr, maddr);
        fflush(stdout);
        return 0;
    }
    //printf("\nR16 A:%08x V:%04x", (u32)addr, (u32)ret);
    return (u32)ret;
}

u32 DCread_flash(struct DC* this, u32 addr, u32 bits)
{
    switch(addr) {

    }
    return *(u32 *)((u8*)this->flash.ptr + (addr & 0x1FFFF));
}

u32 DCread32(void *ptr, u32 addr) {
    THIS;
    i64 ret = -1;
    u32 maddr = addr;
    addr &= 0x1FFFFFFF; // only 29 bits of real addresses I guess
    if (addr < 0x1FFFFF)
        ret = *(u32 *) ((((u8 *) this->BIOS.ptr) + (addr & 0x1FFFFF)));
    if ((addr >= 0x200000 ) && (addr <= 0x21FFFF)) {
        ret = DCread_flash(this, addr, 32);
    }

    if (((addr >= 0x0C000000) && (addr <= 0x0CFFFFFF)) || ((addr >= 0x0E000000) && (addr <= 0x0EFFFFFF)))
        ret = *(u32 *) (this->RAM + (addr & 0xFFFFFF));

    // handle Operand Cache access
    if ((maddr >= 0x7C000000) && (maddr <= 0x7FFFFFFF)) {
        if (this->sh4.regs.CCR.OIX == 0)
            return *(u32 *)(&this->OC[((addr & 0x2000) >> 1) | (addr & 0xFFF)]);
        else
            return *(u32 *)(&this->OC[((addr & 0x02000000) >> 13) | (addr & 0xFFF)]);
    }

    if ((addr >= 0x005F8000) && (addr <= 0x005FFFFF))
        return holly_read(this, addr);

    switch(addr) {
        case 0x005F6900: // Interrupt status register SB_ISTNRM
            // Clear anything that a 1 is written to in bits 21 to 0
#ifdef LYCODER
            return 8; // stub for IP.BIN to bypass vblank wait
#else
        return this->io.SB_ISTNRM;
#endif
        case 0x00702c00: // AICA ARMRST
            return this->aica.ARMRST;
        case 0x1F000024: // EXPEVT
            return this->sh4.regs.EXPEVT;
    }

    switch(maddr) {
        case 0xFFD8000C:
            //printf("\nTCTN0 %d", this->io.TCNT0);
            return this->io.TCNT0;
    }

    if (ret < 0) {
        //dbg_break();
        printf("\nread32 unknown addr %08x %08x cycle%llu", addr, maddr, this->sh4.trace_cycles);
        dbg_printf("\nIT HAPPENED HERE!");
        fflush(stdout);
        return 0;
    }
    //dbg_printf("\nRd32 A:%08X V:%08x", (u32) addr, (u32) ret);
    return ret;
}

void DCwrite8(void *ptr, u32 addr, u32 val)
{
    THIS;
    val &= 0xFF;
    u32 maddr = addr;
    addr &= 0x1FFFFFFF; // only 29 bits of real addresses I guess
    if (((addr >= 0x0C000000) && (addr <= 0x0CFFFFFF)) || ((addr >= 0x0E000000) && (addr <= 0x0EFFFFFF))) {
        this->RAM[addr & 0xFFFFFF] = (u8)val;
        return;
    }

    switch(maddr) {
        case 0xffd80000: // TOCR timer output control register
            this->io.TOCR = 0;
            return;
        case 0xffd80004: // TSTR
            this->io.TSTR = 0;
            return;
        case 0xFF940190: // -0xFF SDMR
            this->io.SDMR = val & 0xFF;
            return;
    }

    // handle Operand Cache access
    if ((maddr >= 0x7C000000) && (maddr <= 0x7FFFFFFF)) {
        if (this->sh4.regs.CCR.OIX == 0)
            this->OC[((addr & 0x2000) >> 1) | (addr & 0xFFF)] = val;
        else
            this->OC[((addr & 0x02000000) >> 13) | (addr & 0xFFF)] = val;
        return;
    }

    printf("\nwrite8 unknown addr %08x %08x val %02x cycle:%llu", addr, maddr, val, this->sh4.trace_cycles);
}

void G1_write(struct DC* this, u32 reg, u32 val, u32 bits)
{
    switch(reg | 0x5F7400) {
        case 0x5F7480: { // SB_G1RRC write-only timing for system ROM accesses
            return;
        }
        case 0x5F74E4: { // Secret GDROM unlock register!
            printf("\nGDROM UNLOCK! %04x cyc:%llu", val, this->sh4.trace_cycles);
            return;
        }
    }
    printf("\nUnhandled G1 reg write %02x val %04x bits %d", reg, val, bits);
}


void DCwrite16(void *ptr, u32 addr, u32 val)
{
    THIS;
    val &= 0xFFFF;
    u32 maddr = addr;
    addr &= 0x1FFFFFFF; // only 29 bits of real addresses I guess
    if ((addr >= 0x05000000) && (addr <= 0x05800000)) { // VRAM 32bit access
        *((u16 *)(&this->VRAM[addr - 0x05000000])) = (u16)val;
        //if ((val != 0) || (dbg.watch));
        //    printf("\nVRAM WRITE A:%08x R:%06x V:%04x", addr, addr - 0x05000000, val & 0xFFFF);
        return;
    }

    if (((addr >= 0x0C000000) && (addr <= 0x0CFFFFFF)) || ((addr >= 0x0E000000) && (addr <= 0x0EFFFFFF))) {
        *((u16 *)&this->RAM[addr & 0xFFFFFF]) = (u16)val;
        return;
        // addr & 0xFFFFFF
    }


        // handle Operand Cache access
    if ((maddr >= 0x7C000000) && (maddr <= 0x7FFFFFFF)) {
        if (this->sh4.regs.CCR.OIX == 0)
            *(u16 *)(&this->OC[((addr & 0x2000) >> 1) | (addr & 0xFFF)]) = val;
        else
            *(u16 *)(&this->OC[((addr & 0x02000000) >> 13) | (addr & 0xFFF)]) = val;
        return;
    }

    if ((addr >= 0x005F7000) && (addr <= 0x005F70FF)) {
        GDROM_write(this, maddr & 0xFF, val, 16);
        return;
    }

    if ((addr >= 0x005F7400) && (addr <= 0x005F74FF)) {
        G1_write(this, maddr & 0xFF, val, 16);
        return;
    }

    switch(maddr) {
        case 0xFF800004: // BCR2
            this->io.BSCR2 = val;
            return;
        case 0xFF800024: // RTCOR
            this->io.RTCOR = val;
            return;
        case 0xFF800028: // RFCR
            // doc a little unclear on this
            this->io.RFCR = 0b1010010000000000 | (val & 0x1FF);
            return;
        case 0xFF80001C: // RTCSR
            this->io.RTCSR = val;
            return;
        case  0xFFD80010:  // TCR0
            return;
    }

    printf("\nwrite16 unknown addr %08x %08x val %02x", addr, maddr, val);
    fflush(stdout);
}


void DCwrite32(void *ptr, u32 addr, u32 val)
{
    THIS;

#ifdef SH4_DBG_SUPPORT
    if (dbg.trace_on) {
        dbg_printf("\ncyc:%05llu write32  addr:%08x val:%08x", this->sh4.trace_cycles, addr, val);
    }
#endif

    u32 maddr = addr;
    if (maddr == 0x8c00b7bc) {
        printf("\nWRITE TO 8C00B7BC cyc:%llu val:%08x PC:%08x", this->sh4.trace_cycles, val, this->sh4.regs.PC);
    }
    addr &= 0x1FFFFFFF;

    if ((addr >= 0x05000000) && (addr <= 0x05800000)) { // VRAM 32bit access
        *((u32 *)(&this->VRAM[addr - 0x05000000])) = val;
        //if ((val != 0) || (dbg.watch))
            //printf("\nVRAM WRITE A:%08x R:%06x V:%08x", addr, addr - 0x05000000, val);
        return;
    }
    if (((addr >= 0x0C000000) && (addr <= 0x0CFFFFFF)) || ((addr >= 0x0E000000) && (addr <= 0x0EFFFFFF))) {
        *((u32 *) (&this->RAM[addr & 0xFFFFFF])) = val;
        return;
    }
    if ((maddr >= 0xE0000000) && (maddr <= 0xE3FFFFFF)) { // store queue write
        *(u32*)&this->sh4.SQ[(addr >> 5) & 1][addr & 0x1C] = val;
    }
    if ((addr >= 0x005F8000) && (addr <= 0x005FFFFF)) {
        return holly_write(this, addr, val);
    }
    if ((addr >= 0x005F7400) && (addr <= 0x005F74FF)) {
        G1_write(this, maddr & 0xFF, val, 32);
        return;
    }

    // handle Operand Cache access
    if ((maddr >= 0x7C000000) && (maddr <= 0x7FFFFFFF)) {
        if (this->sh4.regs.CCR.OIX == 0)
            *(u32*)(&this->OC[((addr & 0x2000) >> 1) | (addr & 0xFFF)]) = val;
        else
            *(u32*)(&this->OC[((addr & 0x02000000) >> 13) | (addr & 0xFFF)]) = val;
        return;
    }

    switch(addr) {
        case 0x1F000010: // MMUCR
            this->io.MMUCR = val;
            return;
        case 0x005F6910: // SB_IML2NRM
            this->io.SB_IML2NRM = val;
            DC_recalc_interrupts(this);
            return;
        case 0x005F6920: // SB_IML4NRM
            this->io.SB_IML4NRM = val;
            DC_recalc_interrupts(this);
            return;
        case 0x005F6930: // SB_IML6NRM
            this->io.SB_IML6NRM = val; //1= enabled
            DC_recalc_interrupts(this);
            return;
        case 0x005F6900: // Interrupt status register SB_ISTNRM
            // Clear anything that a 1 is written to in bits 21 to 0
            printf("\nCLEAR ISTNRM %08x", val);
            this->io.SB_ISTNRM ^= this->io.SB_ISTNRM & val & 0x3FFFFF;
            return;
        case 0x005f6884: // SB_LMMODE0 This register determines the data size when writing to the area from 0x1100 0000 to 0x11FF FFFF in texture memory via the TA FIFO buffer - Direct Texture Path. 0 = 64bit 1 = 32bit
            this->io.SB_LMMODE0 = val & 1;
            return;
        case 0x005f6888: // SB_LMMODE1 This register determines the data size when writing to the area from 0x1300 0000 to 0x13FF FFFF in texture memory via the TA FIFO buffer. The meanings of the settings are the same as for SB_LMMODE0. (The default is also the same.)
            this->io.SB_LMMODE1 = val & 1;
            return;
        case 0x00702c00: // AICA ARMRST
            this->aica.ARMRST = val;
            return;
    }

    switch(maddr) {
        case 0xFF800000: // BSCR
            this->io.BSCR = val;
            printf("\nWRITE TO BSCR!");
            return;
        case 0xFF800008: // WCR1
            this->io.WCR1 = val;
            return;
        case 0xFF80000C: // WCR2
            this->io.WCR2 = val;
            return;
        case 0xFF800014: // MCR
            this->io.MCR = val;
            return;
        case 0xFF940190: // SDMR
            this->io.SDMR = val;
            return;
        case 0xFF000038: // QACR0 for store queues
            this->sh4.regs.QACR0 = val;
            return;
        case 0xFF00003C: // QACR1 for store queues
            this->sh4.regs.QACR1 = val;
            return;
        case 0xffd80008: // TCOR0
            this->io.TCOR0 = val;
            return;
        case 0xffd8000c:
            this->io.TCNT0 = val;
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
    }

    printf("\nwrite32 unknown addr %08x %08x val %02x cycle:%llu", addr, maddr, val, this->sh4.trace_cycles);
    dbg_break();
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