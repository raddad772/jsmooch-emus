//
// Created by . on 1/19/25.
//
#include <cstdlib>
#include <cstring>

#include "helpers/multisize_memaccess.cpp"
#include "nds_bus.h"
#include "nds_vram.h"

namespace NDS {

static constexpr u32 VRAM_offsets[9] = {
        0x00000, // A - 128KB
        0x20000, // B - 128KB
        0x40000, // C - 128KB
        0x60000, // D - 128KB
        0x80000, // E - 64KB
        0x90000, // F - 16KB
        0x94000, // G - 16KB
        0x98000, // H - 32K
        0xA0000  // I - 16K
};

static const u32 VRAM_masks[9] = {
        0x1FFFF, // A - 128KB
        0x1FFFF, // B - 128KB
        0x1FFFF, // C - 128KB
        0x1FFFF, // D - 128KB
        0x0FFFF, // E - 64KB
        0x03FFF, // F - 16KB
        0x03FFF, // G - 16KB
        0x07FFF, // H - 32K
        0x03FFF  // I - 16K
};

static void map_arm9(core *th, u32 addr_start, u32 addr_end, u8 *ptr, u32 wh)
{
    for (u32 addr = addr_start; addr < addr_end; addr += 0x4000) {
        u32 v = (ptr == nullptr) ? 0 : ((addr - addr_start) & VRAM_masks[wh]);
        th->mem.vram.map.arm9[NDSVRAMSHIFT(addr) & NDSVRAMMASK] = ptr+v;
    }
}

static void map_arm7(core *th, u32 which, u8 *ptr)
{
    th->mem.vram.map.arm7[which] = ptr;
}

#define ENG_A 0
#define ENG_B 1

static void map_eng2d_bg_vram(core *th, u32 engnum, u32 addr_start, u32 addr_end, u8 *ptr)
{
    PPU::ENG2D &eng = th->ppu.eng2d[engnum];
    for (u32 addr = addr_start; addr < addr_end; addr += 0x4000) {
        eng.memp.bg_vram[addr >> 14] = ptr;
        if (ptr != nullptr) ptr += 0x4000;
    }
}

static void map_eng2d_obj_vram(core *th, u32 engnum, u32 addr_start, u32 addr_end, u8 *ptr)
{
    PPU::ENG2D &eng = th->ppu.eng2d[engnum];
    for (u32 addr = addr_start; addr < addr_end; addr += 0x4000) {
        eng.memp.obj_vram[addr >> 14] = ptr;
        if (ptr != nullptr) ptr += 0x4000;
    }
}

static void map_eng2d_bg_extended_palette8k(core *th, u32 engnum, u32 slot_start, u32 slot_end, u8 *ptr)
{
    PPU::ENG2D &eng = th->ppu.eng2d[engnum];
    for (u32 slot = slot_start; slot <= slot_end; slot++) {
        eng.memp.bg_extended_palette[slot] = ptr;
        if (ptr != nullptr) ptr += 0x2000;
    }
}

static void map_eng2d_obj_extended_palette(core *th, u32 engnum, u8 *ptr)
{
    PPU::ENG2D &eng = th->ppu.eng2d[engnum];
    eng.memp.obj_extended_palette = ptr;
}

static void map_eng3d_texture_slot(core *th, u32 slot, u8 *ptr)
{
    th->ppu.eng3d.slots.texture[slot] = ptr;
}

static void map_eng3d_palette_slot(core *th, u32 slot_start, u32 slot_end, u8 *ptr)
{
    for (u32 slot = slot_start; slot <= slot_end; slot++) {
        th->ppu.eng3d.slots.palette[slot] = ptr;
        if (ptr != nullptr) ptr += 0x4000;
    }
}

static void set_bankA(core *th, u32 mst, u32 ofs, u8 *ptr)
{
    u32 offset;
    switch(mst) {
        case 0: // ARM9 06800000
            map_arm9(th, 0x06800000, 0x0681FFFF, ptr, NVA);
            break;
        case 1: // ARM9 06000000 & EngA BG-VRAM. + 20,000h * OFS
            offset = 0x20000 * ofs;
            map_arm9(th, 0x06000000+offset,   0x0601FFFF+offset, ptr, NVA);
            map_eng2d_bg_vram(th, ENG_A, offset, 0x1FFFF+offset, ptr);
            break;//
        case 2: // map to ARM9 06400000&EngA_OBJ-VRAM + (20,000h * OFS.0)
            offset = 0x20000 * (ofs & 1);
            map_arm9(th, 0x06400000+offset, 0x0641FFFF+offset, ptr, NVA);
            map_eng2d_obj_vram(th, ENG_A, offset, 0x1FFFF+offset, ptr);
            break;
        case 3: // map to 3DE_texture slot 0-3 by OFS
            map_eng3d_texture_slot(th, ofs, ptr);
            break;
        default:
            NOGOHERE;
    }
}

static void set_bankB(core *th, u32 mst, u32 ofs, u8 *ptr)
{
    u32 offset;
    switch(mst) {
        case 0: // ARM9 06820000
            map_arm9(th, 0x06820000, 0x0683FFFF, ptr, NVB);
            break;
        case 1: // ARM9 06000000 & EngA BG-VRAM. + 20,000h * OFS
            offset = 0x20000 * ofs;
            map_arm9(th, 0x06000000+offset,   0x0601FFFF+offset, ptr, NVB);
            map_eng2d_bg_vram(th, ENG_A, offset, 0x1FFFF+offset, ptr);
            break;//
        case 2: // map to ARM9 06400000&EngA_OBJ-VRAM + (20,000h * OFS.0)
            offset = 0x20000 * (ofs & 1);
            map_arm9(th, 0x06400000+offset, 0x0641FFFF+offset, ptr, NVB);
            map_eng2d_obj_vram(th, ENG_A, offset, 0x1FFFF+offset, ptr);
            break;
        case 3: // map to 3DE_texture slot 0-3 by OFS
            map_eng3d_texture_slot(th, ofs, ptr);
            break;
        default:
        NOGOHERE;
    }
}


static void set_bankC(core *th, u32 mst, u32 ofs, u8 *ptr)
{
    // 128KB
    u32 offset;
    switch(mst) {
        case 0: // ARM9 06840000
            map_arm9(th, 0x06840000, 0x0685FFFF, ptr, NVC);
            break;
        case 1: // ARM9 06000000 & EngA BG-VRAM. + 20,000h * OFS
            offset = 0x20000 * ofs;
            map_arm9(th, 0x06000000 + offset, 0x0601FFFF + offset, ptr, NVC);
            map_eng2d_bg_vram(th, ENG_A, offset, 0x1FFFF + offset, ptr);
            break;
        case 2: //  map to ARM7 06000000 + (20,000h * OFS.0)
            map_arm7(th, ofs & 1, ptr);
            break;
        case 3: // map to 3DE_texture slot 0-3 by OFS
            map_eng3d_texture_slot(th, ofs, ptr);
            break;
        case 4: // map to ARM9 06200000 & EngB_BG-VRAM
            map_arm9(th, 0x06200000, 0x0621FFFF, ptr, NVC);
            map_eng2d_bg_vram(th, ENG_B, 0, 0x1FFFF, ptr);
            break;
        case 5:
        default:
            NOGOHERE;
    }
}

static void set_bankD(core *th, u32 mst, u32 ofs, u8 *ptr)
{
    // 128KB
    u32 offset;
    switch(mst) {
        case 0: // ARM9 06860000
            map_arm9(th, 0x06860000, 0x0687FFFF, ptr, NVD);
            break;
        case 1: // ARM9 06000000 & EngA BG-VRAM. + 20,000h * OFS
            offset = 0x20000 * ofs;
            map_arm9(th, 0x06000000 + offset, 0x0601FFFF + offset, ptr, NVD);
            map_eng2d_bg_vram(th, ENG_A, offset, 0x1FFFF + offset, ptr);
            break;
        case 2: //  map to ARM7 06000000 + (20,000h * OFS.0)
            map_arm7(th, ofs & 1, ptr);
            break;
        case 3: // map to 3DE_texture slot 0-3 by OFS
            map_eng3d_texture_slot(th, ofs, ptr);
            break;
        case 4: // map to ARM9 06600000 & EngB_OBJ-VRAM
            map_arm9(th, 0x06600000, 0x0661FFFF, ptr, NVD);
            map_eng2d_obj_vram(th, ENG_B, 0, 0x1FFFF, ptr);
            break;
        default:
            NOGOHERE;
    }
}

// 06000000 to 06FFFFFF in 16KB segments is 1024. addr >> 14 & 0x17FF
// u8* mem.vram.map.arm9[1024]
// u8* mem.vram.map.arm7[2]
// ARM7 will just be,

// th->mem.vram.map.arm9
// th->mem.vram.map.arm7
// th->ppu.eng2d[0].

/* We have...
 * EngA BG-VRAM:  06000000 mask 7FFFF (512kb)
 * EngA OBJ-VRAM: 06400000 mask 3FFFF (256kb)
 * EngA BG extended palette: slots 0-3, 4k per, 32k total, mask FFF per-slot
 * EngA OBJ extended palette: slot 0. only 8k total, mask 1FFF
 * 3DE Texture, 4 slots, 128KB each
 * 3DE Texture Palette, 6 slots, 16m each
 * EngB BG-VRAM,  06200000  mask 1FFFF
 * EngB OBJ-VRAM, 06600000  mask 1FFFF
 * EngB BG extended palette, slots 0-3, 4k per, 32k total. per-slot mask FFF
 * EngB OBJ extended palette, slot 0, 8KB, mask 1FFF
 * two slots of ARM7, 06000000 and 06200000

 * */

static void set_bankE(core *th, u32 mst, u32 ofs, u8 *ptr)
{
    // 64K
    switch(mst) {
        case 0: // ARM9 06880000
            map_arm9(th, 0x06880000, 0x0688FFFF, ptr, NVE);
            break;
        case 1: // ARM9 06000000&EngA_BG-VRAM+0
            map_arm9(th, 0x06000000, 0x0600FFFF, ptr, NVE);
            map_eng2d_bg_vram(th, ENG_A, 0, 0xFFFF, ptr);
            break;
        case 2: // ARM9 06400000&EngA_OBJ-VRAM+0
            map_arm9(th, 0x06400000, 0x0640FFFF, ptr, NVE);
            map_eng2d_obj_vram(th, ENG_A, 0, 0xFFFF, ptr);
            break;
        case 3: // 3DE texture palette slot 0-3
            map_eng3d_palette_slot(th, 0, 3, ptr);
            break;
        case 4: // EngA BG extended palette (lower 32k)
            map_eng2d_bg_extended_palette8k(th, ENG_A, 0, 3, ptr);
            break;
        default: NOGOHERE;
    }
}

static void set_bankF(core *th, u32 mst, u32 ofs, u8 *ptr)
{
    // 16K
    // MST 0, map to ARM9 06890000
    // MST 1, ARM9&EngA BG-VRAM + 06000000+(4000h*OFS.0)+(10000h*OFS.1)
    // MST 2, ARM9&EngA OBJ-VRAM + 6400000h+(4000h*OFS.0)+(10000h*OFS.1)
    // MST 3, 3DE texture palette Slot (OFS.0*1)+(OFS.1*4) 0, 1, 4, or 5
    // MST 4, EngA BG-extended. OFS=0 slot 0-1, OFS=1 slot 2-3
    // MST 5, EngA OBJ-extended, lower 8K used
    // other MSTs are not allowed
    u32 offset, offset2;
    switch(mst) {
        case 0: // ARM9 06890000
            map_arm9(th, 0x06890000, 0x06893FFF, ptr, NVF);
            break;
        case 1: // ARM9&EngA BG-VRAM + 06000000+(4000h*OFS.0)+(10000h*OFS.1)
            offset = (0x4000*(ofs & 1)) + (0x10000 * ((ofs >> 1) & 1));
            offset2 = 0x4000*(ofs & 1) + 0x10000;
            map_arm9(th, 0x06000000+offset, 0x06003FFF+offset, ptr, NVF);
            map_eng2d_bg_vram(th, ENG_A, offset, 0x3FFF+offset, ptr);
            if (offset != offset2) {
                map_arm9(th, 0x06000000+offset2, 0x06003FFF+offset2, ptr, NVF);
                map_eng2d_bg_vram(th, ENG_A, offset2, 0x3FFF+offset2, ptr);
            }
            break;
        case 2: // ARM9&EngA OBJ-VRAM + 6400000h+(4000h*OFS.0)+(10000h*OFS.1)
            offset = (0x4000*(ofs & 1)) + (0x10000 * ((ofs >> 1) & 1));
            offset2 = 0x4000*(ofs & 1) + 0x10000;
            map_arm9(th, 0x06400000+offset, 0x06403FFF+offset, ptr, NVF);
            map_eng2d_obj_vram(th, ENG_A, offset, 0x3FFF+offset, ptr);
            if (offset2 != offset) {
                map_arm9(th, 0x06400000+offset2, 0x06403FFF+offset2, ptr, NVF);
                map_eng2d_obj_vram(th, ENG_A, offset2, 0x3FFF+offset2, ptr);
            }
            break;
        case 3: // 3DE texture palette Slot (OFS.0*1)+(OFS.1*4) 0, 1, 4, or 5
            offset = (ofs & 1) + ((ofs & 2) << 1);
            map_eng3d_palette_slot(th, offset, offset, ptr);
            break;
        case 4: // EngA BG-extended. OFS=0 slot 0-1, OFS=1 slot 2-3
            offset = (ofs & 1) << 1;
            map_eng2d_bg_extended_palette8k(th, ENG_A, offset, offset + 1, ptr);
            break;
        case 5: // EngA OBJ-extended, lower 8K used
            map_eng2d_obj_extended_palette(th, ENG_A, ptr);
            break;
        default: NOGOHERE;
    }
}

static void set_bankG(core *th, u32 mst, u32 ofs, u8 *ptr)
{
    // 16KB
    // other MSTs are not allowed
    u32 offset, offset2;
    switch(mst) {
        case 0: // ARM9 06894000
            map_arm9(th, 0x06894000, 0x06897FFF, ptr, NVG);
            break;
        case 1: // ARM9&EngA BG-VRAM + 06000000+(4000h*OFS.0)+(10000h*OFS.1)
            offset = (0x4000 * (ofs & 1)) + (0x10000 * ((ofs >> 1) & 1));
            offset2 = 0x4000*(ofs & 1) + 0x10000;
            map_arm9(th, 0x06000000 + offset, 0x06003FFF + offset, ptr, NVG);
            map_eng2d_bg_vram(th, ENG_A, offset, 0x3FFF + offset, ptr);
            if (offset != offset2) {
                map_arm9(th, 0x06000000 + offset2, 0x06003FFF + offset2, ptr, NVG);
                map_eng2d_bg_vram(th, ENG_A, offset2, 0x3FFF + offset2, ptr);
            }
            break;
        case 2: // ARM9&EngA OBJ-VRAM + 6400000h+(4000h*OFS.0)+(10000h*OFS.1)
            offset = (0x4000 * (ofs & 1)) + (0x10000 * ((ofs >> 1) & 1));
            offset2 = 0x4000*(ofs & 1) + 0x10000;
            map_arm9(th, 0x06400000 + offset, 0x06403FFF + offset, ptr, NVG);
            map_eng2d_obj_vram(th, ENG_A, offset, 0x3FFF + offset, ptr);
            if (offset != offset2) {
                map_arm9(th, 0x06400000 + offset2, 0x06403FFF + offset2, ptr, NVG);
                map_eng2d_obj_vram(th, ENG_A, offset2, 0x3FFF + offset2, ptr);
            }
            break;
        case 3: // 3DE texture palette slot (OFS.0*1)+(OFS.1*4) 0, 1, 4, or 5
            offset = (ofs & 1) + ((ofs & 2) << 1);
            map_eng3d_palette_slot(th, offset, offset, ptr);
            break;
        case 4: // EngA BG-extended. OFS=0 slot 0-1, OFS=1 slot 2-3
            offset = (ofs & 1) << 1;
            map_eng2d_bg_extended_palette8k(th, ENG_A, offset, offset + 1, ptr);
            break;
        case 5: // EngA OBJ-extended, lower 8K used
            map_eng2d_obj_extended_palette(th, ENG_A, ptr);
            break;
        default: NOGOHERE;
    }
}

static void set_bankH(core *th, u32 mst, u32 ofs, u8 *ptr)
{
    // 32KB
    // MST 0, map to ARM9 6898000
    // MST 1, ARM9&EngB BG-VRAM 6200000h
    // MST 2, EngB BG extended palette slots 0-3
    // other MST not allowed
    u32 offset, offset2;
    switch(mst) {
        case 0: // ARM9 06898000
            map_arm9(th, 0x06898000, 0x0689FFFF, ptr, NVH);
            break;
        case 1: // ARM9&EngB BG-VRAM 6200000h
            map_arm9(th, 0x06200000, 0x06207FFF, ptr, NVH);
            map_arm9(th, 0x06210000, 0x06217FFF, ptr, NVH);
            map_eng2d_bg_vram(th, ENG_B, 0x0000, 0x7FFF, ptr);
            map_eng2d_bg_vram(th, ENG_B, 0x10000, 0x17FFF, ptr);
            break;
        case 2: // EngB BG extended palette slots 0-3
            map_eng2d_bg_extended_palette8k(th, ENG_B, 0, 3, ptr);
            break;
        default: NOGOHERE;
    }
}

static void set_bankI(core *th, u32 mst, u32 ofs, u8 *ptr)
{
    // 16KB
    // MST 0, map to ARM9 68A0000
    // MST 1, map to ARM9&EngB BG-VRAM 6208000h
    // MST 2, map to ARM9&EngB OBJ-VRAM 06600000h
    // MST 3, EngB BG extended slots 0-3
    switch(mst) {
        case 0: // ARM9 068A0000
            map_arm9(th, 0x068A0000, 0x068A3FFF, ptr, NVI);
            break;
        case 1: // ARM9&EngB BG-VRAM 6208000h
            map_arm9(th, 0x06208000, 0x0620BFFF, ptr, NVI);
            map_arm9(th, 0x06218000, 0x0621BFFF, ptr, NVI);
            map_eng2d_bg_vram(th, ENG_B, 0x8000, 0xBFFF, ptr);
            map_eng2d_bg_vram(th, ENG_B, 0x18000, 0x1BFFF, ptr);
            break;
        case 2: {// ARM9&EngB OBJ-VRAM 06600000h
            for (u32 i = 0; i < 8; i++) {
                u32 offset = i * 0x4000;
                map_arm9(th, 0x06600000+offset, 0x06603FFF+offset, ptr, NVI);
                map_eng2d_obj_vram(th, ENG_B, 0+offset, 0x3FFF+offset, ptr);
            }
            break; }
        case 3: // EngB OBJ extended palette
            map_eng2d_obj_extended_palette(th, ENG_B, ptr);
            break;
        default: NOGOHERE;
    }
}


void core::VRAM_set_bank(u32 bank_num, u32 mst, u32 ofs, u8 *ptr, bool force, bool update_io)
{
    if (force || (mem.vram.io.bank[bank_num].ofs != ofs) || (mem.vram.io.bank[bank_num].mst != mst)) {
        switch (bank_num) {
            case NVA:
                set_bankA(this, mst, ofs, ptr);
                break;
            case NVB:
                set_bankB(this, mst, ofs, ptr);
                break;
            case NVC:
                set_bankC(this, mst, ofs, ptr);
                break;
            case NVD:
                set_bankD(this, mst, ofs, ptr);
                break;
            case NVE:
                set_bankE(this, mst, ofs, ptr);
                break;
            case NVF:
                set_bankF(this, mst, ofs, ptr);
                break;
            case NVG:
                set_bankG(this, mst, ofs, ptr);
                break;
            case NVH:
                set_bankH(this, mst, ofs, ptr);
                break;
            case NVI:
                set_bankI(this, mst, ofs, ptr);
                break;
        }
    }
    if (update_io) {
        mem.vram.io.bank[bank_num].ofs = ofs;
        mem.vram.io.bank[bank_num].mst = mst;
    }
}

void core::VRAM_resetup_banks() {
#define clear(a) memset(&a, 0, sizeof(a))
    clear(mem.vram.map);
    clear(ppu.eng2d[0].memp);
    clear(ppu.eng2d[1].memp);
    clear(ppu.eng3d.slots);
#undef clear

#define setbank(letter,bn) if (mem.vram.io.bank[bn].enable) set_bank##letter(this, mem.vram.io.bank[bn].mst, mem.vram.io.bank[bn].ofs, mem.vram.data + VRAM_offsets[bn]);
    setbank(A,NVA);
    setbank(B,NVB);
    setbank(C,NVC);
    setbank(D,NVD);
    setbank(E,NVE);
    setbank(F,NVF);
    setbank(G,NVG);
    setbank(H,NVH);
    setbank(I,NVI);
#undef setbank
}

u32 core::VRAM_tex_read(const u32 addr, const u8 sz) const {
    // 128KB * up to 4, in up to 4 slots
    // A B C D always occupy one of the slots if it's occupied

    // So first determine bank
    u32 bank = addr >> 17;
    if (!ppu.eng3d.slots.texture[bank]) {
        static int a = 1;
        if (a) {
            printf("\nMiss VRAM read at %06x", addr);
            a = 0;
        }
        return 0;
    }

    return cR[sz](ppu.eng3d.slots.texture[bank], addr & 0x1FFFF);
}

u32 core::VRAM_pal_read(u32 addr, u8 sz) const
{
    // 96k accross 6 slots
    u32 bank = (addr >> 14) & 7;
    if (bank >= 6) bank -= 6;
    if (!ppu.eng3d.slots.palette[bank]) {
        static int a = 1;
        if (a) {
            printf("\nMiss VRAM palette read at %05x", addr);
            a = 0;
        }
        return 0;
    }

    return cR[sz](ppu.eng3d.slots.palette[bank], addr & 0x3FFF);
}
}