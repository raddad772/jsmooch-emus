//
// Created by . on 1/17/25.
//
#include <cstring>

#include "../gba_bus.h"
#include "eeprom.h"

#define STATE_GET_CMD 1
#define STATE_GET_ADDR 2
#define STATE_WRITE 4 // a write is happening to the EEPROM
#define STATE_OUTPUT_JUNK_NIBBLE 8 // consume 4 bits and don't act on them
#define STATE_READ 16 // a read is happening from the EEPROM
#define STATE_CONSUME_BIT 32 // consume 1 bit and don't act on it
namespace GBA::cart {

static inline u32 eeprom_get_bit(persistent_store &store, const u32 bit_num)
{
    const u32 byte_num = bit_num >> 3;
    const u32 shift = 7 - (bit_num & 7);
    const u32 data = static_cast<u8 *>(store.data)[byte_num];
    return (data >> shift) & 1;
}

static inline void eeprom_put_bit(persistent_store &store, const u32 bit_num, const u32 bit)
{
    const u32 byte_num = bit_num >> 3;
    const u32 shift = 7 - (bit_num & 7);
    static_cast<u8 *>(store.data)[byte_num] |= (bit << shift);
}

void core::init_eeprom()
{
    RAM.eeprom.ready_at = 0;
    RAM.eeprom.size_was_detected = 1;
    RAM.eeprom.size_in_bytes = 8192; // Always store 8K even for 512 byte games...

    RAM.eeprom.serial.data = 0;
    RAM.eeprom.serial.sz = 0;
    RAM.eeprom.mode = STATE_GET_CMD;
}

/*
Read, first writes:
2 bits "11" (Read Request)
n bits eeprom address (MSB first, 6 or 14 bits, depending on EEPROM)
1 bit "0"

Then expects:
4 bits junk
64 bits good

Write writes:
2 bits "10" (Write Request)
n bits eeprom address (MSB first, 6 or 14 bits, depending on EEPROM)
64 bits data (conventionally MSB first)
1 bit "0"


 1 1 1 1 1 1 1 0
 1 0 1 1 1 1 0 1
 0 0 0 0 0 0 0 0
 0 0 0 0 0 0 0 0
 0 0 0 0 0 0 0 0
 0 0 0 0 0 0 0 0
 1 1 0 1 1 0 0 1
 0 1 1 0 1 0 0 1
 */

void EEPROM::serial_clear()
{
    serial.data = 0;
    serial.sz = 0;
}

void EEPROM::serial_add(u32 v)
{
    serial.data <<= 1;
    serial.data |= v;
    serial.sz++;
}

static void pprint_RAM_hw_bit0(GBA::core *th, u32 addr, u32 numbits, u32 was_read)
{
    // 8 per line!
    u32 num = 0;
    u32 on_line = 8;
    printf("\nPPRINT START AT %08x", addr);
    while (num<numbits) {
        if (on_line == 8) {
            on_line = 0;
            printf("\n");
        }
        printf(" %d", GBA::core::mainbus_read(th, addr+(num*2), 2, 0, 0) & 1);
        num++;
        on_line++;
    }
}


void core::write_eeprom(u32 addr, u8 sz, u8 access, u32 val)
{
    auto &e = RAM.eeprom;
    gba->waitstates.current_transaction += gba->waitstates.sram;
    if (e.mode & STATE_GET_CMD) {
        // Add bit to buffer
        // If buffer size is 2, start next command...
        // If it is a read command, detect size
        e.serial_add(val & 1);
        if (e.serial.sz == 2) {
            if (e.serial.data == 3) { // Read request!
                e.addr_bus_size = gba->dma.channel[3].io.word_count - 3;
                e.mode = STATE_GET_ADDR | STATE_CONSUME_BIT | STATE_OUTPUT_JUNK_NIBBLE | STATE_READ;
                e.serial_clear();
                return;
            }
            else if (e.serial.data == 2) {
                e.addr_bus_size = gba->dma.channel[3].io.word_count - 67;
                e.mode = STATE_GET_ADDR | STATE_WRITE | STATE_CONSUME_BIT;
                e.serial_clear();
                return;
            }
            else {
                printf("\nEEPROM: got BAD CMD!? %lld", e.serial.data);
                e.serial_clear();
            }
        }
    }
    else if (e.mode & STATE_GET_ADDR) {
        // Push bits of address into buffer
        e.serial_add(val);
        if (e.serial.sz == e.addr_bus_size) {
            // Address get!
            e.cmd.cur_bit_addr = ((e.serial.data << 3) & 0x1FFF) << 3;
            e.mode &= ~STATE_GET_ADDR;
            e.serial_clear();
            if (e.mode & STATE_READ) {
            }
            else {
                memset(static_cast<u8 *>(RAM.store->data) + (e.cmd.cur_bit_addr >> 3), 0, 8);
            }
        }
        return;
    }
    else if (e.mode & STATE_READ) {
        if (e.mode & STATE_CONSUME_BIT) {
            e.mode &= ~STATE_CONSUME_BIT;
            return;
        }
        return;
    }
    else if (e.mode & STATE_WRITE) {
        // Write a bit...
        if (e.serial.sz < 64) {
            eeprom_put_bit(*RAM.store, e.cmd.cur_bit_addr, val & 1);
            e.cmd.cur_bit_addr++;
            e.serial.sz++;
            return;
        }
        if (e.mode & STATE_CONSUME_BIT) {
            e.mode &= ~STATE_CONSUME_BIT;
            e.mode &= ~STATE_WRITE;
            e.serial_clear();
            e.mode = STATE_GET_CMD;
            e.ready_at = gba->clock_current() + 100000;
            RAM.store->dirty = true;
            //pprint_RAM_hw_bit0(dma[3].op.src_addr - (64 * 2), 64, 0);
        }
        else {
            printf("\nUNKNOWN POST-WRITE WRITES!?!?!");
        }
    }
    else {
        printf("\nWAIT WHAT? %d", e.mode);
    }
}

u32 core::read_eeprom(u32 addr, u8 sz, u8 access, bool has_effect)
{
    auto &e = RAM.eeprom;
    gba->waitstates.current_transaction += gba->waitstates.sram;
    if (e.mode & STATE_WRITE) {
        printf("\nEEPROM: READ DURING WRITE OP!?");
        return 1;
    }
    else if (e.mode & STATE_READ) {
        if (e.mode & STATE_OUTPUT_JUNK_NIBBLE) {
            e.serial.sz++;
            if (e.serial.sz == 4) {
                e.mode &= ~STATE_OUTPUT_JUNK_NIBBLE;
                e.serial_clear();
            }
            return 0;
        }
        u32 v = eeprom_get_bit(*RAM.store, e.cmd.cur_bit_addr);
        e.cmd.cur_bit_addr++;
        e.serial.sz++;
        if (e.serial.sz == 64) {
            e.mode &= ~STATE_READ;
            if (e.mode) printf("\nLeftover mode:%d", e.mode);
            e.mode = STATE_GET_CMD;
            // Finish up read...
            //pprint_RAM_hw_bit0(dma[3].op.dest_addr - (63 * 2), 64, 1);
            e.serial_clear();
        }
        return v;
    }
    return gba->clock_current() >= RAM.eeprom.ready_at;
}

}