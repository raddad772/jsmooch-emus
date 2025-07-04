//
// Created by . on 8/30/24.
//

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>


#include "iou.h"
#include "mmu.h"

static void aux_card_RAMwrite(struct apple2* this, u32 addr, u8 val)
{
    assert(1==2);
}

static u8 slot_read(struct apple2* this, u32 addr)
{
    printf("\nSLOT READ %04x cyc:%lld", addr, this->clock.master_cycles);
    return 0;
}

static u8 aux_card_RAMread(struct apple2* this, u32 addr)
{
    printf("\nAUX RAM READ %04x cyc:%lld", addr, this->clock.master_cycles);
    return 0;
}

static void apple2_MMU_reset_io(struct apple2* this)
{
    memset(&this->mmu.io, 0, sizeof(struct APL2SS));
}

void apple2_MMU_reset(struct apple2* this)
{
    this->mmu.RAM_bank = 0xD000; // 0xC000 or 0xD000
    this->mmu.page1_accesses = 0;
    this->mmu.io.WRTCOUNT = 0;
    apple2_MMU_reset_io(this);
    printf("\nMMU RESET!");
}

static u8 read_cxxx(struct apple2* this, u32 addr, u32 is_write, u8 old_val, u32 has_effect)
{
    // access C3XX with SLOTC3ROM reset, will enabled SLOTC8ROM,
    //  which fill fix motherboard ROM to C800-CFFF.
    // This is then canceled by reading CFFF or on reset
    if ((addr >= 0xC300) && (addr < 0xC400)) {
        if (has_effect && (this->mmu.io.SLOTC3ROM == 0)) this->mmu.io.INTC8ROM = 1;
    }

    i32 r = -1;

    u32 INTC8ROMWAS = this->mmu.io.INTC8ROM;

    if (has_effect && (addr == 0xCFFF))
        this->mmu.io.INTC8ROM = 0;


    if ((addr >= 0xC800) && INTC8ROMWAS)
        return this->mmu.ROM.ptr[addr & 0xFFF];

    /*
 CXXX will not go high if
INTCXROM is set and $C100—$CFFF is addressed,
ifSLOTC3ROM is reset and $C3XX is addressed,or
if INTC8O0M is set and $C800-$CFFFis addres-
sed.
     */

    u32 things = (this->mmu.io.INTCXROM << 1) | this->mmu.io.SLOTC3ROM;
    u32 group0 = ((addr >= 0xC100) && (addr < 0xC300)) || (addr >= 0xC400);
    switch(things) {
        case 0: // INTCXROM=0, SLOTC3ROM=0
            if (group0) return slot_read(this, addr);
            return this->mmu.ROM.ptr[addr & 0xFFF];
        case 1: // INTCXROM=0, SLOTC3ROM=1
            return slot_read(this, addr);
        case 2: // INTCXROM=1, SLOTC3ROM=0
        case 3: // INTCXROM=1, SLOTC3ROM=1
            return this->mmu.ROM.ptr[addr & 0xFFF];
        default:
            assert(1==2);
    }
    NOGOHERE;
    return 0;
}

static u8 access_c0xx(struct apple2* this, u32 addr, u32 is_write, u8 old_val, u32 has_effect)
{
    // 80STORE
    u32 r = 0;
    u32 MSB = 0;
    u32 caddr = addr & 0xFFFE;
    switch (caddr) {
        #define RW(addr, component, name) case addr: if (has_effect)  this-> component.io. name = addr & 1; break;
        #define W(addr, component, name) case addr: if (has_effect && is_write) this-> component.io. name = addr & 1; break;
        W(0xC000, mmu, STORE80);
        W(0xC002, mmu, RAMRD);
        W(0xC004, mmu, RAMWRT);
        W(0xC006, mmu, INTCXROM);
        W(0xC008, mmu, ALTZP);
        W(0xC00A, mmu, SLOTC3ROM);
        W(0xC00C, iou, COL80);
        W(0xC00E, iou, ALTCHRSET);
        RW(0xC050, iou, TEXT);
        RW(0xC052, iou, MIXED);
        RW(0xC054, iou, PAGE2);
        RW(0xC056, iou, HIRES);
        RW(0xC058, iou, AN0);
        RW(0xC05A, iou, AN1);
        RW(0xC05C, iou, AN2);
        RW(0xC05E, iou, AN3);
        #undef W
        #undef RW
    }

    if (!is_write) {
        #define R(addr, component, name) case addr: MSB = this-> component.io. name << 7; break;
        switch(addr) {
            R(0xC013, mmu, RAMRD);
            R(0xC014, mmu, RAMWRT);
            R(0xC015, mmu, INTCXROM);
            R(0xC016, mmu, ALTZP);
            R(0xC017, mmu, SLOTC3ROM);
            R(0xC018, mmu, STORE80);
            R(0xC01A, iou, TEXT);
            R(0xC01B, iou, MIXED);
            R(0xC010, iou, AKD);
            R(0xC01E, iou, ALTCHRSET);
            R(0xC01F, iou, COL80);
            R(0xC01C, iou, PAGE2);
            R(0xC01D, iou, HIRES);
            case 0xC011: MSB = (this->mmu.io.BANK1 ^ 1) << 7; break;
            case 0xC019: MSB = (this->iou.io.VBL ^ 1) << 7; break;
            R(0xC012, mmu, HRAMRD);
        }
        #undef R
    }

    // Do C08X shenanigans
#define SBANK1 { this->mmu.RAM_bank = 0xC000; this->mmu.io.BANK1 = 1; }
#define SBANK2 { this->mmu.RAM_bank = 0xD000; this->mmu.io.BANK1 = 0; }
#define BANKS if (addr & 8) SBANK1 else SBANK2
    if ((addr >= 0xC080) && (addr < 0xC090)) {
        // last two bits = 11 or 00, enable
        // last two bits = 01 or 10, disable
        switch(addr) {
            case 0xC080: // read enable  0000
            case 0xC088: // read enable  1000
            case 0xC084: // read enable  0100
            case 0xC08C: // read enable  1100
            case 0xC082: // rd disable 0010
            case 0xC08A: // rd disable 1010
            case 0xC086: // rd disable 0110
            case 0xC08E: // rd disable 1110
                BANKS
                this->mmu.io.HRAMRD = (((addr & 3) == 0) || ((addr & 3) == 3));
                this->mmu.io.WRTCOUNT = 0;
                this->mmu.io.HRAMWRT = 0;
                break;
            case 0xC081: // rd disable 0001
            case 0xC089: // rd disable 1001
            case 0xC085: // rd disable 0101
            case 0xC08D: // rd disable 1101
            case 0xC083: // rd enalbe  0011
            case 0xC08B: // rd enable  1011
            case 0xC087: // rd enable  0111
            case 0xC08F: // rd enable  1111
                BANKS;
                this->mmu.io.HRAMRD = (((addr & 3) == 0) || ((addr & 3) == 3));
                if (!is_write) {
                    this->mmu.io.WRTCOUNT++;
                    if (this->mmu.io.WRTCOUNT >= 2) this->mmu.io.HRAMWRT = 1;
                }
                else {
                    this->mmu.io.WRTCOUNT = 0;
                }
                break;
        }
    }

    // IOU softswitches...
    apple2_IOU_access_c0xx(this, addr, has_effect, is_write, &r, &MSB);

    // Handle reading from hardware...

    return r | MSB;
}

static u32 addr_is_aux(struct apple2* this, u32 addr, u32 RDWRT) {
    // 0-0x1FF, always main RAM
    if (addr < 0x200) {
        if (this->mmu.io.ALTZP) return 1;
        return 0;
    }
    // 0x200-0x3FF, affected by RAMRD
    if (addr < 0x400)
        return RDWRT;
    // 0x400-0x7FF
    if (addr < 0x800) {
        if (this->mmu.io.STORE80)
            return this->iou.io.PAGE2;
        return RDWRT;
    }
    // 0x800-0x1FFF, RAMRD only...
    if (addr < 0x2000)
        return RDWRT;
    // 0x2000-0x3FFF, RAMRD unless HIRES
    if (addr < 0x4000) {
        if (this->mmu.io.STORE80 && this->iou.io.HIRES)
            return this->iou.io.PAGE2;
        return RDWRT;
    }
    if (addr >= 0xD000)
        return this->mmu.io.ALTZP;

    return RDWRT;
}

u8 apple2_cpu_bus_read(struct apple2* this, u32 addr, u8 old_val, u32 has_effect)
{
    if (has_effect) {
        if ((addr >= 0x100) && (addr < 0x200)) {
            this->mmu.page1_accesses++;
        }
        else {
            if ((this->mmu.page1_accesses >= 3) && (addr == 0xFFFC)) apple2_MMU_reset(this);
            this->mmu.page1_accesses = 0;
        }
    }
    if (addr < 0xC000) {
        if (addr_is_aux(this, addr, this->mmu.io.RAMRD))
            return aux_card_RAMread(this, addr);
        return this->mmu.RAM.ptr[addr];
    }
    if (addr < 0xC100) {
        return access_c0xx(this, addr, 0, old_val, has_effect);
    }
    if (addr < 0xD000) {
        return read_cxxx(this, addr, 0, old_val, has_effect);
    }
    if (addr < 0xE000) { // Two 4k banks, plus BIOS
        if (!this->mmu.io.HRAMRD) return this->mmu.ROM.ptr[addr & 0x7FFF];
        return this->mmu.RAM.ptr[(addr & 0xFFF) | this->mmu.RAM_bank];
    }
    if (addr_is_aux(this, addr, this->mmu.io.RAMRD)) return aux_card_RAMread(this, addr);
    if (!this->mmu.io.HRAMRD) return this->mmu.ROM.ptr[addr & 0x7FFF];
    return this->mmu.RAM.ptr[addr];
}

void apple2_cpu_bus_write(struct apple2* this, u32 addr, u8 val)
{
    if (addr < 0xC000) {
        if (addr_is_aux(this, addr, this->mmu.io.RAMWRT))
            aux_card_RAMwrite(this, addr, val);
        else this->mmu.RAM.ptr[addr] = val;
        return;
    }
    if (addr < 0xC100) {
        access_c0xx(this, addr, 1, 0, 1);
        return;
    }
    if (addr < 0xD000) {
        read_cxxx(this, addr, 0, 0, 1);
        return;
    }
    if (addr < 0xE000) {
        if (this->mmu.io.HRAMWRT) this->mmu.RAM.ptr[(addr & 0xFFF) | this->mmu.RAM_bank] = val;
        return;
    }
    if (addr_is_aux(this, addr, this->mmu.io.RAMWRT)) aux_card_RAMwrite(this, addr, val);
    if (this->mmu.io.HRAMWRT) this->mmu.RAM.ptr[addr] = val;
}
