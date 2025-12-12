//
// Created by . on 12/4/24.
//

#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "helpers/debug.h"
#include "helpers/physical_io.h"
#include "helpers/multisize_memaccess.cpp"

#include "../nds_bus.h"
#include "nds_cart.h"
#include "../nds_irq.h"
#include "../nds_dma.h"
#include "../nds_debugger.h"
#include "nds_eeprom.h"
#include "nds_flash.h"

namespace NDS::CART {

static constexpr u32 masksz[5] = { 0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF };

void ridge::reset()
{
    // TODO: this
    backup.status.busy = 0;
}

void ridge::direct_boot()
{
    data_mode = NCDM_main;
    io.romctrl.data_block_size = 1;
}

bool ridge::load_ROM_from_RAM(char* fil, u64 fil_sz, physical_io_device *pio, u32 *SRAM_enable) {
    ROM.allocate(fil_sz);
    memcpy(ROM.ptr, fil, fil_sz);

    if (SRAM_enable) *SRAM_enable = 1;
    backup.store = &pio->cartridge_port.SRAM;
    backup.store->fill_value = 0xFF;
    persistent_store *ps = &pio->cartridge_port.SRAM;
    ps->requested_size = 1 * 1024 * 1024;
    ps->persistent = true;
    return true;
}

u32 ridge::read_spicnt() const {
    u32 v = io.spi.divider_val;
    v |= io.spi.next_chipsel << 6;
    v |= io.spi.slot_mode << 13;
    v |= io.transfer_ready_irq << 14;
    v |= io.nds_slot_enable << 15;
    // 0 = ready, 1 = busy
    if (io.spi.slot_mode == 0) v |= io.romctrl.busy << 7;
    return v;
}

bool ridge::data_ready()
{
    return io.romctrl.busy;
}

u32 ridge::read_romctrl() const {
    return io.romctrl.u;
}

void ridge::raise_transfer_irq() {
    bus->update_IFs_card(IRQ_CART_DATA_READY);
    printf("\nRAISE TRANSFER IRQ...");
}

u32 ridge::get_transfer_irq_bits() const {
    return io.transfer_ready_irq && !io.romctrl.busy;
}

void ridge::set_block_start_status(u32 val, u32 transfer_ready_irq)
{
    u32 old_bits = get_transfer_irq_bits();
    io.romctrl.busy = val;
    io.transfer_ready_irq = transfer_ready_irq;
    u32 new_bits = get_transfer_irq_bits();

    if (!old_bits && new_bits)
        raise_transfer_irq();
}

static u32 rom_transfer_time(u32 clk_spd, u8 sz_in_bytes)
{
    static constexpr u32 cpb[2] = {5, 8};
    return cpb[clk_spd] * sz_in_bytes;
}

u32 ridge::read_spi(u32 bnum) const {
    return backup.data_out.b8[bnum];
}

u32 ridge::read_rom(u32 addr, u8 sz)
{
    u32 output = 0xFFFFFFFF;

    if(!io.romctrl.data_ready) {
        return output;
    }

    if (cmd.sz_out != 0) {
        output = cmd.data_out[cmd.pos_out++ % cmd.sz_out];
    }
    else
        cmd.pos_out++;

    io.romctrl.data_ready = 0;

    if (cmd.pos_out == cmd.sz_out) {
        if (cmd.data_in[0] == 0xB7) dbgloglog_bus(NDS_CAT_CART_READ_COMPLETE, DBGLS_INFO, "Finish read of %d words %d %d", cmd.sz_out, cmd.pos_out, cmd.sz_out);
        io.romctrl.busy = 0;
        cmd.pos_out = 0;
        cmd.sz_out = 0;

        if (io.transfer_ready_irq) {
            NDS_update_IFs_card(IRQ_CART_DATA_READY);
        }
    } else {
        rom_busy_until = bus->clock.current7() + rom_transfer_time(io.romctrl.transfer_clk_rate, 4);
        if (sch_sch) bus->scheduler.delete_if_exist(sch_id);
        bus->scheduler.add_or_run_abs(rom_busy_until, ANB_after_read, this, &ridge::check_transfer, &sch_sch);
    }

    return output;
}

u32 ridge::get_block_size() const
{
    u32 v = io.romctrl.data_block_size;
    switch(v) {
        case 0:
            return 0;
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            return 64 << v;
        case 7:
            return 1;
    }
    return 0;
}

void pprint_mem(void *ptr, u32 num_bytes, u32 addr)
{
    printf("\nROM at addr:%08x", addr);
    u8 *p = (u8 *)ptr;
    for (u32 i = 0; i < num_bytes; i++) {
        printf(" %02x", p[i]);
    }
}

void ridge::handle_cmd()
{
    cmd.pos_out = 0;
    cmd.sz_out = 0;
    io.romctrl.data_ready = 0;

    cmd.sz_out = get_block_size();
    switch(cmd.data_in[0]) {
        case 0xB7: { // Read!
            u32 address = cmd.data_in[1] << 24;
            address |= cmd.data_in[2] << 16;
            address |= cmd.data_in[3] <<  8;
            address |= cmd.data_in[4] <<  0;

            address %= ROM.size;

            if(address <= 0x7FFF) {
                address = 0x8000 + (address & 0x1FF);
            }

#ifndef MIN
#define MIN(a,b) ((a) < (b)) ? (a) : (b)
#endif
            cmd.sz_out = MIN(0x80, cmd.sz_out);

            dbgloglog_bus(NDS_CAT_CART_READ_START, DBGLS_INFO, "Start read of %d words from cart:%06x", cmd.sz_out, address);

            memcpy(cmd.data_out, static_cast<char *>(ROM.ptr)+address, cmd.sz_out * 4);
            //pprint_mem(cmd.data_out, 4, address);
            break;
        }
        case 0xB8: { // rom chip ID
            cmd.sz_out = 1;
            cmd.data_out[0] = 0x1FC2;
            break;
        }
        default: {
            printf("\nUNHANDLED CMD %02x", cmd.data_in[0]);
            break;
        }
    }

    io.romctrl.busy = cmd.sz_out != 0;

    if (io.romctrl.busy) {

        rom_busy_until = bus->clock.current7() + rom_transfer_time(io.romctrl.transfer_clk_rate, 4);
        if (sch_sch) bus->scheduler.delete_if_exist(sch_id);
        bus->scheduler.add_or_run_abs(rom_busy_until, ANB_after_read, this, &ridge::check_transfer, &sch_sch);
    }
    else if(io.transfer_ready_irq) {
        NDS_update_IFs_card(IRQ_CART_DATA_READY);
    }
}

void ridge::after_read()
{
    io.romctrl.data_ready = 1;
    if (bus->io.rights.nds_slot_is7) NDS_trigger_dma7_if(2);
    else bus->trigger_dma9_if(DMA_DS_CART_SLOT);
}

void ridge::check_transfer(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<ridge *>(ptr);
    switch(key) {
        case ANB_none:
            return;
        case ANB_handle_cmd:
            th->handle_cmd();
            return;
        case ANB_after_read:
            th->after_read();
            return;
    }
}

void ridge::spi_write_spicnt(u32 val, u32 bnum)
{
    if (bnum == 0) {
        io.spi.divider_val = val & 3;
        switch (io.spi.divider_val) {
            case 0:
                io.spi.divider = 8;
                break;
            case 1:
                io.spi.divider = 16;
                break;
            case 2:
                io.spi.divider = 32;
                break;
            case 3:
                io.spi.divider = 64;
                break;
        }
        io.spi.next_chipsel = (val >> 6) & 1;
    }
    else {
        io.spi.slot_mode = (val >> 5) & 1;
        io.nds_slot_enable = (val >> 7) & 1;
        io.transfer_ready_irq = (val >> 6) & 1;
    }
}

void ridge::inc_addr()
{
    backup.cmd_addr = (backup.cmd_addr & 0xFFFFFF00) | ((backup.cmd_addr + 1) & 0xFF);
}


void ridge::spi_transaction(u32 val)
{
    switch(backup.detect.kind) {
        case BK_none:
            printf("\nSPI transactions with no backup!?");
            return;
        case BK_flash:
            //printf("\nflash transact: %02x", val);
            flash_spi_transaction(val);
            return;
        case BK_eeprom:
            //printf("\neeprom transact...");
            eeprom_spi_transaction(val);
            return;
        case BK_fram:
            printf("\nFRAM not implement!");
            return;
    }
}

void ridge::write_romctrl(u32 val)
{
    u32 mask = 0x7FFFFFFF;
    io.romctrl.u = (val & 0x7FFFFFFF) | (io.romctrl.u & 0x80000000);
    if ((val & 0x80000000) && !io.romctrl.busy) {
        // Trigger handle of command
        io.romctrl.busy = 1;

        rom_busy_until = bus->clock.current7() + rom_transfer_time(io.romctrl.transfer_clk_rate, 8);
        if (sch_sch) bus->scheduler.delete_if_exist(sch_id);
        bus->scheduler.add_or_run_abs(rom_busy_until, ANB_handle_cmd, this, &ridge::check_transfer, &sch_sch);
    }
}

void ridge::write_cmd(u32 addr, u32 val)
{
    cmd.data_in[addr] = val;
}


#define FS_SUBSYS 0xB
void ridge::detect_kind(u32 from, u32 val)
{
    // subsystem tag (bits 0-4) and the data portion (bits 6-31)
    u32 subsystem = val & 0x1F;
    printf("\nARM9 subsystem %1x", subsystem);
    if (!backup.detect.done) {
        u32 reset = 1;
        if (subsystem == FS_SUBSYS) {
            u32 data = val >> 6;
            if ((backup.detect.pos == 0) && (from == 9) && (data == 0)) {
                reset = 0;
            }
            else if ((backup.detect.pos < 3) && (from == 9) && (!backup.detect.done)) {
                reset = 0;
                backup.detect.arg_buf_addr = data;
                u32 second_word = cR32(bus->mem.RAM, (backup.detect.arg_buf_addr + 4) & 0x3FFFFF);
                // bits 0-1 encode the savedata type (0=none, 1=EEPROM, 2=flash, 3=fram
                // - never seen fram used), and
                // bits 8-15 encode the backup size in a shift amount - i.e. the size in bytes is (1 << n)
                u32 kind = second_word & 3;
                u8 sz = 1 << ((second_word >> 8) & 0xFF);
                printf("\nDETECT CART SAVE KIND:%d SZ:%d bytes", kind, sz);
                backup.detect.kind = static_cast<backup_kind>(kind);
                backup.detect.sz = sz;
                backup.detect.sz_mask = sz - 1;
                backup.detect.done = 1;
                backup.store->requested_size = sz;
                switch(backup.detect.kind) {
                    case 0: // none
                        break;
                    case 1: // eeprom
                        NDS_eeprom_setup();
                        break;
                    case 2: // flash
                        NDS_flash_setup();
                        break;
                    case 3: // fram
                        printf("\nFRAM IMPLEMENT!");
                        break;
                }
            }
            else if ((backup.detect.pos == 1) && (from == 7) && (data == 1)) {
                reset = 0;
            }
            /*if ((backup.detect.pos == 3) && (from == 7) && (data == 1)) {
                reset = 0;
                printf("\nSTEP 4");
            }
            if ((backup.detect.pos == 4) && (from == 9)) {
                printf("\nSTEP 5?");
            }*/
        }

        if (reset) {
            backup.detect.pos = 0;
        }
        else {
            backup.detect.pos++;
        }
    }
}
}