#if defined (__clang__)
#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnreachableCode"
#endif
//
//
// Created by Dave on 2/13/2024.
//

#include <cassert>
#include "dc_bus.h"
#include "dc_mem.h"
#include "holly.h"
#include "g2.h"
#include "helpers/elf_helpers.h"

#include "helpers/multisize_memaccess.cpp"

//#define func_printf(...) printf(__VA_ARGS__)
#define func_printf(...) (void)0

#define QUIT_ON_TOO_MANY 10
//#define SH4MEM_BRK 0x005F7018
//#define SH4_INS_BRK 0x80000000

#define B32(b31_b28, b27_24,b23_20,b19_16,b15_12,b11_8,b7_4,b3_0) ( \
  ((0b##b31_b28) << 28) | ((0b##b27_24) << 24) | \
  ((0b##b23_20) << 20) | ((0b##b19_16) << 16) | \
  ((0b##b15_12) << 12) | ((0b##b11_8) << 8) | \
  ((0b##b7_4) << 4) | (0b##b3_0))

namespace DC {

void core::write_C2DST(u32 val)
{
    if (io.SB_C2DST) { // TA FIFO DMA!
        printf(DBGC_GREEN "\nTA FIFO DMA START! %llu", trace_cycles);
        u32 addr = io.SB_C2DSTAT;
        if (addr == 0) addr = 0x10000000;
        // TA polygon FIFO
        if (((addr >= 0x10000000) && (addr <= 0x107FFFE0)) ||
            (addr >= 0x12000000) && (addr <= 0x127FFFE0)) {
            holly.TA_FIFO_DMA(sh4.regs.DMAC_SAR2, io.SB_C2DLEN, VRAM, HOLLY_VRAM_SIZE);
        }
        else {
            printf(DBGC_RED "\nUNSUPPORTED CH2 DMA TO %08x" DBGC_RST, addr);
        }
        io.SB_C2DST = 0;
    }

}

void core::write_SDST(u32 val)
{
    if (val & 1)
        printf(DBGC_RED "\nSORT DMA START REQUEST" DBGC_RST);
}

#define MTHIS auto *th = static_cast<core *>(ptr)

 u64 read_empty(void* ptr, u32 addr, u8 sz, bool *success)
 {
    MTHIS;
    *success = false;

    printf("\nRead empty addr %08x full:%08X", (addr & 0x1FFFFFFF), addr);
    fflush(stdout);
    return 0;
 }

 void write_empty(void* ptr, u32 addr, u64 val, u8 sz, bool* success)
 {
     MTHIS;
     printf("\nWrite empty addr %08x full:%08X val:%08llx", (addr & 0x1FFFFFFF), addr, val);
     fflush(stdout);
     *success = false;
 }

u64 core::aica_read(u32 addr, u8 sz, bool* success) {
    return cR[sz](aica.mem, addr & 0x7FFF);
}

void core::aica_write(u32 addr, u64 val, u8 sz, bool* success)
{
    cW[sz](aica.mem, addr & 0x7FFF, val);
}

u32 core::RTC_read()
{
    // Dreamcast epoch is 1950-something, ours is 1970, so...add 20 years!
    time_t t = time(nullptr);
    return (t + 631152000) & 0xFFFFFFFF;
}

u64 pread_area0(void* ptr, u32 addr, u8 sz, bool* success) {
    MTHIS;
    return th->read_area0(addr, sz, success);
}

u64 core::read_area0(u32 addr, u8 sz, bool* success) {
     u32 full_addr = addr;
     addr &= 0x1FFFFFFF; // only 29 bits of real addresses I guess
     if (addr <= 0x001FFFFF)
         return cR[sz](BIOS.ptr, addr & 0x1FFFFF);
     if ((addr >= 0x00200000) && (addr <= 0x0021FFFF))
         return read_flash(addr, success, sz);

     if ((addr >= 0x005F8000) && (addr <= 0x005FFFFF))
         return holly.read(addr, success);

     if ((addr >= 0x005F6C00) && (addr <= 0x005F6CFF)) { // Maple commands!
         return maple.read(full_addr, sz, success);
     }

     if ((addr >= 0x005F7800) && (addr <= 0x005F78FF)) {
         return G2_read(full_addr, sz, success);
     }

     if ((addr >= 0x005F7C00) && (addr <= 0x005F7CFF)) {
         return G2_read(full_addr, sz, success);
     }

     if ((addr >= 0x00600000) && (addr <= 0x006FFFFF))
         return 0; // asynchronous modem area

     addr &= 0x1FFFFFFF;
     if ((addr >= 0x005F7000) && (addr <= 0x005F70FF)) { // General G1 commands
         return G1_read(full_addr, sz, success);
     }

     if ((addr >= 0x005F7400) && (addr <= 0x005F74FF)) { // GDROM commands
         return G1_read(full_addr, sz, success);
     }

     // handle Operand Cache access
     if ((full_addr >= 0x7C000000) && (full_addr <= 0x7FFFFFFF)) {
         if (sh4.regs.CCR.OIX == 0)
             return cR[sz](sh4.OC, ((addr & 0x2000) >> 1) | (addr & 0xFFF));
         else
             return cR[sz](sh4.OC, ((addr & 0x02000000) >> 13) | (addr & 0xFFF));
     }

     if ((addr >= 0x00700000) && (addr <= 0x00707FE0)) {
         return aica_read(addr, sz, success);
     }

     if ((addr >= 0x00800000) && (addr <= 0x009FFFFF)) {
         return cR[sz](aica.wave_mem, addr & 0x1FFFFF);
     }


     switch(addr) {
         case 0x005F689C: // SB_REVISION
             return 0x0B;
         case 0x005F688C: // SB_FFST
            // REICAST here
             sb.SB_FFST_rc++;
             if (sb.SB_FFST_rc & 0x8)
             {
                 sb.SB_FFST ^= 31;
             }
             return sb.SB_FFST; // does the fifo status has really to be faked ?
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/area0_reads.cpp"
     }

     switch(full_addr) {
         case 0xA0710000: // upper 16 bits of RTC
             return RTC_read() >> 16;
         case 0xA0710004: // lower 16 bits of RTC
             return RTC_read() & 0xFFFF;

     }

     *success = false;
     return 0;
 }

 void pwrite_area0(void* ptr, u32 addr, u64 val, u8 sz, bool* success) {
    auto *th = static_cast<core *>(ptr);
    th->write_area0(addr, val, sz, success);
}

void core::write_area0(u32 addr, u64 val, u8 sz, bool* success) {
    u32 full_addr = addr;
    addr &= 0x1FFFFFFF;

     if (addr <= 0x001FFFFF) {
         dbg_break("DC BIOS write", trace_cycles);
         printf("\nBIOS write %08x %llu", addr, trace_cycles);
         return;
     }
    if ((full_addr >= 0x7C000000) && (full_addr <= 0x7FFFFFFF)) {
         if (sh4.regs.CCR.OIX == 0)
             cW[sz](sh4.OC, ((full_addr & 0x2000) >> 1) | (full_addr & 0xFFF), val);
         else
             cW[sz](sh4.OC, ((full_addr & 0x02000000) >> 13) | (full_addr & 0xFFF), val);
         return;
     }

     addr &= 0x1FFFFFFF;
     if ((addr >= 0x005F7000) && (addr <= 0x005F70FF)) { // General G1 commands
         G1_write(full_addr, val, sz, success);
         return;
     }

     if ((addr >= 0x005F7400) && (addr <= 0x005F74FF)) { // GDROM commands
         G1_write(full_addr, val, sz, success);
         return;
     }

     if ((addr >= 0x005F7800) && (addr <= 0x005F78FF)) {
         G2_write(full_addr, val, sz, success);
         return;
     }

     if ((addr >= 0x005F7C00) && (addr <= 0x005F7CFF)) {
         G2_write(full_addr, val, sz, success);
         return;
     }

     if ((addr >= 0x005F6C00) && (addr <= 0x005F6CFF)) { // Maple commands!
         maple.write(full_addr, val, sz, success);
         return;
     }

     if ((addr >= 0x005F8000) && (addr <= 0x005FFFFF)) {
         holly.write(addr, val, success);
         return;
     }

     if ((addr >= 0x00700000) && (addr <= 0x00707FE0)) {
         aica_write(addr, val, sz, success);
         return;
     }

     if ((addr >= 0x00800000) && (addr <= 0x009FFFFF)) {
         cW[sz](aica.wave_mem, addr & 0x1FFFFF, val);
         return;
     }

     switch(addr) {
#include "generated/area0_writes.cpp"
     }

     *success = false;
 }

u64 pread_area1(void* ptr, u32 addr, u8 sz, bool* success) {
    MTHIS;
    return th->read_area1(addr, sz, success);
}

u64 core::read_area1(u32 addr, u8 sz, bool* success) {
    u32 full_addr = addr;
    addr &= 0x1FFFFFFF; // only 29 bits of real addresses I guess

    if ((addr >= 0x05000000) && (addr <= 0x05800000)) // VRAM access
        return cR[sz](VRAM, addr & 0x007FFFFF);

    *success = false;
    return 0;
}

void pwrite_area1(void* ptr, u32 addr, u64 val, u8 sz, bool* success) {
    MTHIS;
    th->write_area1(addr, val, sz, success);
}

void core::write_area1(u32 addr, u64 val, u8 sz, bool* success) {
    addr &= 0x1FFFFFFF; // only 29 bits of real addresses I guess
    if ((addr >= 0x04000000) && (addr <= 0x04800000)) {
        cW[sz](VRAM, addr & 0x007FFFFF, val);
        return;
    }
    if ((addr >= 0x05000000) && (addr <= 0x05800000)) { // VRAM 32bit access
        cW[sz](VRAM, addr & 0x007FFFFF, val);
        return;
    }

    *success = false;
}

u64 pread_area3(void* ptr, u32 addr, u8 sz, bool* success) {
    MTHIS;
    return th->read_area3(addr, sz, success);
}

u64 core::read_area3(u32 addr, u8 sz, bool* success) {
    addr &= 0x1FFFFFFF;
    if (((addr >= 0x0C000000) && (addr <= 0x0DFFFFFF)))// || ((addr >= 0x0E000000) && (addr <= 0x0EFFFFFF)))
        return cR[sz](RAM, addr & 0xFFFFFF);

    *success = false;
    return 0;
}

void pwrite_area3(void* ptr, u32 addr, u64 val, u8 sz, bool* success) {
    MTHIS;
    th->write_area3(addr, val, sz, success);
}

void core::write_area3(u32 addr, u64 val, u8 sz, bool* success) {
    addr &= 0x1FFFFFFF;
    if (((addr >= 0x0C000000) && (addr <= 0x0DFFFFFF))) {//|| ((addr >= 0x0E000000) && (addr <= 0x0EFFFFFF))) {
        cW[sz](RAM, addr & 0xFFFFFF, val);
        return;
    }

    *success = false;
}

u64 pread_area4(void* ptr, u32 addr, u8 sz, bool* success) {
    MTHIS;
    return th->read_area4(addr, sz, success);
}

u64 core::read_area4(u32 addr, u8 sz, bool* success) {
    assert(1==0);
    return 0;
}

void pwrite_area4(void* ptr, u32 addr, u64 val, u8 sz, bool* success) {
    MTHIS;
    th->write_area4(addr, val, sz, success);
}

void core::write_area4(u32 addr, u64 val, u8 sz, bool* success) {
    assert(1==0);
}


void core::mem_init()
{
    for (u32 i = 0; i < 0x40; i++) {
        mem.read[i] = &read_empty;
        mem.write[i] = &write_empty;
        mem.wptr[i] = nullptr;
        mem.rptr[i] = nullptr;
    }

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4113) // warning C4113: 'u64 (__cdecl *)(void *,u32,u32,u32 *)' differs in parameter lists from 'u64 (__cdecl *)(void *,u32,DC_MEM_SIZE,u32 *)'
#endif

#define MAP(addr, rdfunc, wrfunc, obj) mem.read[(addr)>>2] = (rdfunc); mem.write[(addr)>>2] = (wrfunc); mem.rptr[(addr) >> 2] = ((void *)obj); mem.wptr[(addr) >> 2] = ((void *)obj)
    // All of P0. P1, P2, P3 should mirror the lower half of P0?
    u32 areas[5] = { 0x00, 0x60, 0x80, 0xA0, 0xC0 };
    for (u32 i = 0; i < 5; i++) {
        u32 area = areas[i];
        MAP(area + 0x00, &pread_area0, &pwrite_area0, this);
        MAP(area + 0x04, &pread_area1, &pwrite_area1, this);
        //MAP(0x08, read_area[1], write_area[1]);
        MAP(area + 0x0C, &pread_area3, &pwrite_area3, this);
        MAP(area + 0x10, &pread_area4, &pwrite_area4, this);
        //MAP(0x14, read_area5, write_area5);
        //MAP(0x18, read_area6, write_area6);
        MAP(area + 0x1C, sh4mem.read, sh4mem.write, &sh4);
    }

    for (u32 addr = 0xE0; addr < 0x100; addr += 4) {
        MAP(addr, sh4mem.read, sh4mem.write, &sh4);
    }
#undef MAP
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

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


void core::mainbus_write(u32 addr, u64 val, u8 sz) {
    bool success = true;
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
    mem.write[addr >> 26](mem.wptr[addr>>26], addr, val, sz, &success);

    if (!success) {
        //printf("\nwrite%d unknown addr %08x %08x val %02llu cycle:%llu", dcms(sz), addr & 0x1FFFFFFF, addr, val,
        //       sh4.clock.trace_cycles);
        if (addr >= 0xFF000000)
            func_printf("\n0x%08X: UKN%08X\nu32\naccess_32, rw\n", addr, addr);
        else
            func_printf("\n0x%08X: \nu32\naccess_32, rw\n", addr & 0x1FFFFFFF);
        //fflush(stdout);
        dbg.var++;
#ifdef QUIT_ON_TOO_MANY
        if (dbg.var > QUIT_ON_TOO_MANY) {
            dbg_break("TOO MANY BAD ACCESS", trace_cycles);
        }
#endif
    }
}

u64 core::mainbus_read(u32 addr, u8 sz, bool is_ins_fetch) {
    bool success = true;
    u64 ret = mem.read[addr >> 26](mem.rptr[addr>>26], addr, sz, &success) & VMASK[sz];
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
            dbg_break("TOO MANY BAD ACCESS", trace_cycles);
        }
#endif
        return 0;
    }
    return ret;
}

u64 core::read_flash(u32 addr, bool *success, u8 sz)
{
    *success = true;
    u32 full_addr = addr;
    addr &= 0x1FFFF;
    if (sz == 1) {
        switch (addr) {
            case 0x1A002:
            case 0x1A0A2:
                if (settings.region <= 2)
                    return '0' + settings.region;
                break;
            case 0x1A003:
            case 0x1A0A3:
                if (settings.language <= 5)
                    return '0' + settings.language;
                break;
            case 0x1A004:
            case 0x1A0A4: {
#if defined(LYCODER) || defined(REICAST_DIFF)
                return 0x30;
#endif
                if (settings.broadcast <= 3)
                    return '0' + settings.broadcast;
                break;
            }
        }
    }
    return cR[sz](flash.buf.ptr, addr);
    //return *(u32 *)((u8*)flash.buf.ptr + (addr & 0x1FFFF));
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

u32 core::fetch_ins(u32 addr) {
    u32 val = mainbus_read(addr, 2, true);
#ifdef DC_ELF_PRINT_FUNCS
#ifdef DC_SUPPORT_ELF
    THIS;
    struct elf_symbol32* sym = elf_symbol_list32_find(&elf_symbols, addr, 0x1FFFFFFF);
#ifdef SH4_INS_BRK
    if (addr == SH4_INS_BRK) dbg_break();
#endif
#ifdef DC_ELF_NO_LOOP_SYMBOLS
    if ((sym != nullptr) && (sym->name[0] != 'L') && (sym->name[1] != '_')) {

#else
    if (sym != nullptr) {
#endif
        char buf[500];
        char *mptr = buf;
        mptr += sprintf(mptr, "\n--func %s", sym->name);
        mptr = ptr_seek(mptr, mptr - buf, 20);
        mptr += sprintf(mptr, "(%s)", sym->fname);
        mptr = ptr_seek(mptr, mptr - buf, 32);
        mptr += sprintf(mptr, "PC:%08x R4:%08x R5:%08x R6:%08x cycl:%lld", sh4.regs.PC, sh4.regs.R[4], sh4.regs.R[5], sh4.regs.R[6], sh4.clock.trace_cycles);
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

u64 core::G1_read(u32 addr, u8 sz, bool* success)
{
    addr &= 0x1FFFFFFF;
    if ((addr >= 0x005F7000) && (addr <= 0x005F709C)) {
        return gdrom.read(addr, sz, success);
    }

    switch(addr) {
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/g1_reads.cpp"
    }

    *success = false;
    return 0;
}

void core::G1_write(u32 addr, u64 val, u8 sz, bool* success)
{
    addr &= 0x1FFFFFFF;
    if ((addr >= 0x005F7000) && (addr <= 0x005F709C)) {
        gdrom.write(addr, val, sz, success);
        return;
    }
    switch(addr) {
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/g1_writes.cpp"
        case 0x005F74E4: { // Secret GDROM unlock register!
            return;
        }
    }
    *success = false;
    printf("\nUnhandled G1 reg write %02x val %04llx (%d)", addr, val, sz);
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
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}