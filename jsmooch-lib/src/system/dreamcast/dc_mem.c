#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnreachableCode"
//
//
// Created by Dave on 2/13/2024.
//

#include <time.h>
#include "assert.h"
#include "stdio.h"
#include "dc_mem.h"
#include "holly.h"
#include "gdrom.h"
#include "maple.h"
#include "g2.h"
#include "helpers/elf_helpers.h"

#include "helpers/multisize_memaccess.c"

//#define func_printf(...) printf(__VA_ARGS__)
#define func_printf(...) (void)0

#define QUIT_ON_TOO_MANY 10
//#define SH4MEM_BRK 0x005F7018
//#define SH4_INS_BRK 0x80000000

u64 DCread_flash(struct DC* this, u32 addr, u32 *success, u32 sz);
void G1_write(struct DC* this, u32 addr, u64 val, u32 bits, u32* success);
u64 G1_read(struct DC* this, u32 addr, u32 sz, u32* success);

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
    if (this->g1.SB_GDST) printf("\nGDROM DMA REQUEST!");
}

static void DC_write_C2DST(struct DC* this, u32 val)
{
    if (this->io.SB_C2DST) { // TA FIFO DMA!
        printf(DBGC_GREEN "\nTA FIFO DMA START! %llu", this->sh4.clock.trace_cycles);
        u32 addr = this->io.SB_C2DSTAT;
        if (addr == 0) addr = 0x10000000;
        // TA polygon FIFO
        if (((addr >= 0x10000000) && (addr <= 0x107FFFE0)) ||
            (addr >= 0x12000000) && (addr <= 0x127FFFE0)) {
            holly_TA_FIFO_DMA(this, this->sh4.regs.DMAC_SAR2, this->io.SB_C2DLEN, this->VRAM, HOLLY_VRAM_SIZE);
        }
        else {
            printf(DBGC_RED "\nUNSUPPORTED CH2 DMA TO %08x" DBGC_RST, addr);
        }
        this->io.SB_C2DST = 0;
    }

}

static void DC_write_SDST(struct DC* this, u32 val)
{
    if (val & 1)
        printf(DBGC_RED "\nSORT DMA START REQUEST" DBGC_RST);
}

#define MTHIS struct DC* this = (struct DC*)ptr

 u64 read_empty(void* ptr, u32 addr, enum DC_MEM_SIZE sz, u32 *success)
 {
    MTHIS;
    *success = 0;

    printf("\nRead empty addr %08x full:%08X", (addr & 0x1FFFFFFF), addr);
    fflush(stdout);
    return 0;
 }

 void write_empty(void* ptr, u32 addr, u64 val, enum DC_MEM_SIZE sz, u32* success)
 {
     MTHIS;
     printf("\nWrite empty addr %08x full:%08X val:%08llx", (addr & 0x1FFFFFFF), addr, val);
     fflush(stdout);
     *success = 0;
 }

static u64 aica_read(struct DC* this, u32 addr, u32 sz, u32* success) {
    return cR[sz](this->aica.mem, addr & 0x7FFF);
}

static void aica_write(struct DC* this, u32 addr, u64 val, u32 sz, u32* success)
{
    cW[sz](this->aica.mem, addr & 0x7FFF, val);
}

static u32 RTC_read(struct DC* this)
{
    // Dreamcast epoch is 1950-something, ours is 1970, so...add 20 years!
    time_t t = time(NULL);
    return (t + 631152000) & 0xFFFFFFFF;
}

u64 read_area0(void* ptr, u32 addr, enum DC_MEM_SIZE sz, u32* success)
 {
     MTHIS;
     u32 full_addr = addr;
     addr &= 0x1FFFFFFF; // only 29 bits of real addresses I guess
     if (addr <= 0x001FFFFF)
         return cR[sz](this->BIOS.ptr, addr & 0x1FFFFF);
     if ((addr >= 0x00200000) && (addr <= 0x0021FFFF))
         return DCread_flash(this, addr, success, sz);

     if ((addr >= 0x005F8000) && (addr <= 0x005FFFFF))
         return holly_read(this, addr, success);

     if ((addr >= 0x005F6C00) && (addr <= 0x005F6CFF)) { // Maple commands!
         return maple_read(this, full_addr, sz, success);
     }

     if ((addr >= 0x005F7800) && (addr <= 0x005F78FF)) {
         return G2_read(this, full_addr, sz, success);
     }

     if ((addr >= 0x005F7C00) && (addr <= 0x005F7CFF)) {
         return G2_read(this, full_addr, sz, success);
     }



     if ((addr >= 0x00600000) && (addr <= 0x006FFFFF))
         return 0; // asynchronous modem area

     addr &= 0x1FFFFFFF;
     if ((addr >= 0x005F7000) && (addr <= 0x005F70FF)) { // General G1 commands
         return G1_read(this, full_addr, sz, success);
     }

     if ((addr >= 0x005F7400) && (addr <= 0x005F74FF)) { // GDROM commands
         return G1_read(this, full_addr, sz, success);
     }


     // handle Operand Cache access
     if ((full_addr >= 0x7C000000) && (full_addr <= 0x7FFFFFFF)) {
         if (this->sh4.regs.CCR.OIX == 0)
             return cR[sz](this->sh4.OC, ((addr & 0x2000) >> 1) | (addr & 0xFFF));
         else
             return cR[sz](this->sh4.OC, ((addr & 0x02000000) >> 13) | (addr & 0xFFF));
     }

     if ((addr >= 0x00700000) && (addr <= 0x00707FE0)) {
         return aica_read(this, addr, sz, success);
     }

     if ((addr >= 0x00800000) && (addr <= 0x009FFFFF)) {
         return cR[sz](this->aica.wave_mem, addr & 0x1FFFFF);
     }


     switch(addr) {
         case 0x005F689C: // SB_REVISION
             return 0x0B;
         case 0x005F688C: // SB_FFST
            // REICAST here
             this->sb.SB_FFST_rc++;
             if (this->sb.SB_FFST_rc & 0x8)
             {
                 this->sb.SB_FFST ^= 31;
             }
             return this->sb.SB_FFST; // does the fifo status has really to be faked ?
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/area0_reads.c"
     }

     switch(full_addr) {
         case 0xA0710000: // upper 16 bits of RTC
             return RTC_read(this) >> 16;
         case 0xA0710004: // lower 16 bits of RTC
             return RTC_read(this) & 0xFFFF;

     }

     *success = 0;
     return 0;
 }

 void write_area0(void* ptr, u32 addr, u64 val, enum DC_MEM_SIZE sz, u32* success)
 {
    MTHIS;
    u32 full_addr = addr;
    addr &= 0x1FFFFFFF;

     if (addr <= 0x001FFFFF) {
         dbg_break();
         printf("\nBIOS write %08x %llu", addr, this->sh4.clock.trace_cycles);
         return;
     }
    if ((full_addr >= 0x7C000000) && (full_addr <= 0x7FFFFFFF)) {
         if (this->sh4.regs.CCR.OIX == 0)
             cW[sz](this->sh4.OC, ((full_addr & 0x2000) >> 1) | (full_addr & 0xFFF), val);
         else
             cW[sz](this->sh4.OC, ((full_addr & 0x02000000) >> 13) | (full_addr & 0xFFF), val);
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

     if ((addr >= 0x005F7800) && (addr <= 0x005F78FF)) {
         G2_write(this, full_addr, val, sz, success);
         return;
     }

     if ((addr >= 0x005F7C00) && (addr <= 0x005F7CFF)) {
         G2_write(this, full_addr, val, sz, success);
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

     if ((addr >= 0x00700000) && (addr <= 0x00707FE0)) {
         aica_write(this, addr, val, sz, success);
         return;
     }

     if ((addr >= 0x00800000) && (addr <= 0x009FFFFF)) {
         cW[sz](this->aica.wave_mem, addr & 0x1FFFFF, val);
         return;
     }

     switch(addr) {
#include "generated/area0_writes.c"
     }

     *success = 0;
 }

u64 read_area1(void* ptr, u32 addr, enum DC_MEM_SIZE sz, u32* success)
{
    MTHIS;
    u32 full_addr = addr;
    addr &= 0x1FFFFFFF; // only 29 bits of real addresses I guess

    if ((addr >= 0x05000000) && (addr <= 0x05800000)) // VRAM access
        return cR[sz](this->VRAM, addr & 0x007FFFFF);

    *success = 0;
    return 0;
}

void write_area1(void* ptr, u32 addr, u64 val, enum DC_MEM_SIZE sz, u32* success)
{
    MTHIS;
    addr &= 0x1FFFFFFF; // only 29 bits of real addresses I guess
    if ((addr >= 0x04000000) && (addr <= 0x04800000)) {
        cW[sz](this->VRAM, addr & 0x007FFFFF, val);
        return;
    }
    if ((addr >= 0x05000000) && (addr <= 0x05800000)) { // VRAM 32bit access
        cW[sz](this->VRAM, addr & 0x007FFFFF, val);
        return;
    }

    *success = 0;
}

u64 read_area3(void* ptr, u32 addr, enum DC_MEM_SIZE sz, u32* success)
{
    MTHIS;
    addr &= 0x1FFFFFFF;
    if (((addr >= 0x0C000000) && (addr <= 0x0DFFFFFF)))// || ((addr >= 0x0E000000) && (addr <= 0x0EFFFFFF)))
        return cR[sz](this->RAM, addr & 0xFFFFFF);

    *success = 0;
    return 0;
}

void write_area3(void* ptr, u32 addr, u64 val, enum DC_MEM_SIZE sz, u32* success)
{
    MTHIS;
    addr &= 0x1FFFFFFF;
    if (((addr >= 0x0C000000) && (addr <= 0x0DFFFFFF))) {//|| ((addr >= 0x0E000000) && (addr <= 0x0EFFFFFF))) {
        cW[sz](this->RAM, addr & 0xFFFFFF, val);
        return;
    }

    *success = 0;
}

u64 read_area4(void* ptr, u32 addr, enum DC_MEM_SIZE sz, u32* success)
{
    MTHIS;
    assert(1==0);
    return 0;
}

void write_area4(void* ptr, u32 addr, u64 val, enum DC_MEM_SIZE sz, u32* success)
{
    MTHIS;
    assert(1==0);
}


void DC_mem_init(struct DC* this)
{
    for (u32 i = 0; i < 0x40; i++) {
        this->mem.read[i] = &read_empty;
        this->mem.write[i] = &write_empty;
        this->mem.wptr[i] = NULL;
        this->mem.rptr[i] = NULL;
    }

#define MAP(addr, rdfunc, wrfunc, obj) this->mem.read[(addr)>>2] = (rdfunc); this->mem.write[(addr)>>2] = (wrfunc); this->mem.rptr[(addr) >> 2] = ((void *)obj); this->mem.wptr[(addr) >> 2] = ((void *)obj)
    // All of P0. P1, P2, P3 should mirror the lower half of P0?
    u32 areas[5] = { 0x00, 0x60, 0x80, 0xA0, 0xC0 };
    for (u32 i = 0; i < 5; i++) {
        u32 area = areas[i];
        MAP(area + 0x00, &read_area0, &write_area0, this);
        MAP(area + 0x04, &read_area1, &write_area1, this);
        //MAP(0x08, read_area[1], write_area[1]);
        MAP(area + 0x0C, &read_area3, &write_area3, this);
        MAP(area + 0x10, &read_area4, &write_area4, this);
        //MAP(0x14, read_area5, write_area5);
        //MAP(0x18, read_area6, write_area6);
        MAP(area + 0x1C, this->sh4mem.read, this->sh4mem.write, &this->sh4);
    }

    for (u32 addr = 0xE0; addr < 0x100; addr += 4) {
        MAP(addr, this->sh4mem.read, this->sh4mem.write, &this->sh4);
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
        "\n" DBGC_READ " read64   (%08x) %016llx" DBGC_RST
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
#ifdef SH4MEM_BRK
    if (addr == SH4MEM_BRK) {
        printf(WFORM[sz], WFO);
    }
#endif

#ifdef SH4_DBG_SUPPORT
    if (dbg.trace_on) {
        dbg_printf(WFORM[sz], WFO);
    }
#endif
#ifdef DO_LAST_TRACES
    dbg_LT_printf(WFORM[sz], WFO);
    dbg_LT_endline();
#endif
    this->mem.write[addr >> 26](this->mem.wptr[addr>>26], addr, val, sz, &success);

    if (!success) {
        //printf("\nwrite%d unknown addr %08x %08x val %02llu cycle:%llu", dcms(sz), addr & 0x1FFFFFFF, addr, val,
        //       this->sh4.clock.trace_cycles);
        if (addr >= 0xFF000000)
            func_printf("\n0x%08X: UKN%08X\nu32\naccess_32, rw\n", addr, addr);
        else
            func_printf("\n0x%08X: \nu32\naccess_32, rw\n", addr & 0x1FFFFFFF);
        //fflush(stdout);
        dbg.var++;
#ifdef QUIT_ON_TOO_MANY
        if (dbg.var > QUIT_ON_TOO_MANY) {
            dbg_break();
        }
#endif
    }
}

u64 DCread(void *ptr, u32 addr, u32 sz, bool is_ins_fetch)
{
    THIS;
    u32 success = 1;
    u64 ret = this->mem.read[addr >> 26](this->mem.rptr[addr>>26], addr, sz, &success) & VMASK[sz];
#ifdef SH4_DBG_SUPPORT
    if (!is_ins_fetch) {
        if (dbg.trace_on) {
            dbg_printf(RFORM[sz], RFO);
        }
    }
#endif
#ifdef SH4MEM_BRK
    if (addr == SH4MEM_BRK) {
        printf(RFORM[sz], RFO);
    }
#endif

#ifdef DO_LAST_TRACES
    if (!is_ins_fetch) {
        dbg_LT_printf(RFORM[sz], RFO);
        dbg_LT_endline();
    }
#endif

    if (!success) {
        func_printf("\n(READ) %08x", addr);
        if (addr >= 0xFF000000)
            func_printf("\n0x%08X: UKN%08X\nu32\naccess_32, rw\n", addr, addr);
        else
            func_printf("\n0x%08X: \nu32\naccess_32, rw\n", addr & 0x1FFFFFFF);
        //fflush(stdout);
        dbg.var++;
#ifdef QUIT_ON_TOO_MANY
        if (dbg.var > QUIT_ON_TOO_MANY) {
            dbg_break();
        }
#endif
        return 0;
    }
    return ret;
}


u64 DCread_flash(struct DC* this, u32 addr, u32 *success, u32 sz)
{
    *success =  1;
    u32 full_addr = addr;
    addr &= 0x1FFFF;
    if (sz == 1) {
        switch (addr) {
            case 0x1A002:
            case 0x1A0A2:
                if (this->settings.region <= 2)
                    return '0' + this->settings.region;
                break;
            case 0x1A003:
            case 0x1A0A3:
                if (this->settings.language <= 5)
                    return '0' + this->settings.language;
                break;
            case 0x1A004:
            case 0x1A0A4: {
#if defined(LYCODER) || defined(REICAST_DIFF)
                return 0x30;
#endif
                if (this->settings.broadcast <= 3)
                    return '0' + this->settings.broadcast;
                break;
            }
        }
    }
    return cR[sz](this->flash.buf.ptr, addr);
    //return *(u32 *)((u8*)this->flash.buf.ptr + (addr & 0x1FFFF));
}

static char *ptr_seek(char *ptr, u32 current, u32 pos)
{
    if (pos <= current) return ptr;
    for (u32 i = 0; i < (pos - current); i++) {
        *ptr = ' ';
        ptr++;
        *ptr = 0;
    }
    return ptr;
}

u32 DCfetch_ins(void *ptr, u32 addr)
{
    u32 val = DCread(ptr, addr, 2, true);
#ifdef DC_ELF_PRINT_FUNCS
#ifdef DC_SUPPORT_ELF
    THIS;
    struct elf_symbol32* sym = elf_symbol_list32_find(&this->elf_symbols, addr, 0x1FFFFFFF);
#ifdef SH4_INS_BRK
    if (addr == SH4_INS_BRK) dbg_break();
#endif
#ifdef DC_ELF_NO_LOOP_SYMBOLS
    if ((sym != NULL) && (sym->name[0] != 'L') && (sym->name[1] != '_')) {

#else
    if (sym != NULL) {
#endif
        char buf[500];
        char *mptr = buf;
        mptr += sprintf(mptr, "\n--func %s", sym->name);
        mptr = ptr_seek(mptr, mptr - buf, 20);
        mptr += sprintf(mptr, "(%s)", sym->fname);
        mptr = ptr_seek(mptr, mptr - buf, 32);
        mptr += sprintf(mptr, "PC:%08x R4:%08x R5:%08x R6:%08x cycl:%lld", this->sh4.regs.PC, this->sh4.regs.R[4], this->sh4.regs.R[5], this->sh4.regs.R[6], this->sh4.clock.trace_cycles);
        func_printf("%s", buf);
        //printf("%s", buf);
    }
#endif
#endif
#ifdef SH4_TRACE_INS_FETCH
#ifdef DO_LAST_TRACES
    dbg_LT_printf(DBGC_TRACE "\n fetchi   (%08x) %04x" DBGC_RST, addr, val);
#else
    dbg_printf(DBGC_TRACE "\n fetchi   (%08x) %04x" DBGC_RST, addr, val);
#endif
#endif
    return val;
}

u64 DCread_noins(void *ptr, u32 addr, u32 sz)
{
    return DCread(ptr, addr, sz, false);
}

u64 G1_read(struct DC* this, u32 addr, u32 sz, u32* success)
{
    addr &= 0x1FFFFFFF;
    if ((addr >= 0x005F7000) && (addr <= 0x005F709C)) {
        return GDROM_read(this, addr, sz, success);
    }

    switch(addr) {
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/g1_reads.c"
    }

    *success = 0;
    return 0;
}

void G1_write(struct DC* this, u32 addr, u64 val, u32 bits, u32* success)
{
    addr &= 0x1FFFFFFF;
    if ((addr >= 0x005F7000) && (addr <= 0x005F709C)) {
        GDROM_write(this, addr, val, bits, success);
        return;
    }
    switch(addr) {
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/g1_writes.c"
        case 0x005F74E4: { // Secret GDROM unlock register!
            return;
        }
    }
    *success = 0;
    printf("\nUnhandled G1 reg write %02x val %04llx bits %d", addr, val, bits);
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