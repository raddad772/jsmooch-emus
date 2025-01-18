//
// Created by . on 1/17/25.
//
#include <string.h>

#include "../gba_bus.h"
#include "eeprom.h"

#define STATE_GET_CMD 1
#define STATE_GET_ADDR 2
#define STATE_WRITE 4 // a write is happening to the EEPROM
#define STATE_OUTPUT_JUNK_NIBBLE 8 // consume 4 bits and don't act on them
#define STATE_READ 16 // a read is happening from the EEPROM
#define STATE_CONSUME_BIT 32 // consume 1 bit and don't act on it


static inline u32 eeprom_get_bit(struct persistent_store *store, u32 bit_num)
{
    u32 byte_num = bit_num >> 3;
    u32 shift = 7 - (bit_num & 7);
    u32 data = ((u8 *)store->data)[byte_num];
    u32 v = (data >> shift) & 1;
    return v;
}

static inline void eeprom_put_bit(struct persistent_store *store, u32 bit_num, u32 bit)
{
    u32 byte_num = bit_num >> 3;
    u32 shift = 7 - (bit_num & 7);
    ((u8 *)store->data)[byte_num] |= (bit << shift);
}

void GBA_cart_init_eeprom(struct GBA_cart *this)
{
    this->RAM.eeprom.ready_at = 0;
    this->RAM.eeprom.size_was_detected = 1;
    this->RAM.eeprom.size_in_bytes = 8192; // Always store 8K even for 512 byte games...

    this->RAM.eeprom.serial.data = 0;
    this->RAM.eeprom.serial.sz = 0;
    this->RAM.eeprom.mode = STATE_GET_CMD;
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

static void serial_clear(struct GBA_CART_EEPROM *e)
{
    e->serial.data = 0;
    e->serial.sz = 0;
}

static void serial_add(struct GBA_CART_EEPROM *e, u32 v)
{
    e->serial.data <<= 1;
    e->serial.data |= v;
    e->serial.sz++;
}

static void pprint_RAM_hw_bit0(struct GBA *this, u32 addr, u32 numbits, u32 was_read)
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
        printf(" %d", GBA_mainbus_read(this, addr+(num*2), 2, 0, 0) & 1);
        num++;
        on_line++;
    }
}


void GBA_cart_write_eeprom(struct GBA*this, u32 addr, u32 sz, u32 access, u32 val)
{
    struct GBA_CART_EEPROM *e = &this->cart.RAM.eeprom;
    if (e->mode & STATE_GET_CMD) {
        // Add bit to buffer
        // If buffer size is 2, start next command...
        // If it is a read command, detect size
        serial_add(e, val & 1);
        if (e->serial.sz == 2) {
            if (e->serial.data == 3) { // Read request!
                e->addr_bus_size = this->dma[3].io.word_count - 3;
                e->mode = STATE_GET_ADDR | STATE_CONSUME_BIT | STATE_OUTPUT_JUNK_NIBBLE | STATE_READ;
                serial_clear(e);
                return;
            }
            else if (e->serial.data == 2) {
                e->addr_bus_size = this->dma[3].io.word_count - 67;
                e->mode = STATE_GET_ADDR | STATE_WRITE | STATE_CONSUME_BIT;
                serial_clear(e);
                return;
            }
            else {
                printf("\nEEPROM: got BAD CMD!? %lld", e->serial.data);
                serial_clear(e);
            }
        }
    }
    else if (e->mode & STATE_GET_ADDR) {
        // Push bits of address into buffer
        serial_add(e, val);
        if (e->serial.sz == e->addr_bus_size) {
            // Address get!
            e->cmd.cur_bit_addr = ((e->serial.data * 8) & 0x1FFF) * 8;
            e->mode &= ~STATE_GET_ADDR;
            serial_clear(e);
            if (e->mode & STATE_READ) {
            }
            else {
                memset(((u8 *)this->cart.RAM.store->data) + (e->cmd.cur_bit_addr >> 3), 0, 8);
            }
        }
        return;
    }
    else if (e->mode & STATE_READ) {
        if (e->mode & STATE_CONSUME_BIT) {
            e->mode &= ~STATE_CONSUME_BIT;
            return;
        }
        return;
    }
    else if (e->mode & STATE_WRITE) {
        // Write a bit...
        if (e->serial.sz < 64) {
            eeprom_put_bit(this->cart.RAM.store, e->cmd.cur_bit_addr, val & 1);
            e->cmd.cur_bit_addr++;
            e->serial.sz++;
            return;
        }
        if (e->mode & STATE_CONSUME_BIT) {
            e->mode &= ~STATE_CONSUME_BIT;
            e->mode &= ~STATE_WRITE;
            serial_clear(e);
            e->mode = STATE_GET_CMD;
            e->ready_at = GBA_clock_current(this) + 100000;
            this->cart.RAM.store->dirty = 1;
            //pprint_RAM_hw_bit0(this, this->dma[3].op.src_addr - (64 * 2), 64, 0);
        }
        else {
            printf("\nUNKNOWN POST-WRITE WRITES!?!?!");
        }
    }
    else {
        printf("\nWAIT WHAT? %d", e->mode);
    }
}

u32 GBA_cart_read_eeprom(struct GBA*this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    struct GBA_CART_EEPROM *e = &this->cart.RAM.eeprom;
    if (e->mode & STATE_WRITE) {
        printf("\nEEPROM: READ DURING WRITE OP!?");
        return 1;
    }
    else if (e->mode & STATE_READ) {
        if (e->mode & STATE_OUTPUT_JUNK_NIBBLE) {
            e->serial.sz++;
            if (e->serial.sz == 4) {
                e->mode &= ~STATE_OUTPUT_JUNK_NIBBLE;
                serial_clear(e);
            }
            return 0;
        }
        u32 v = eeprom_get_bit(this->cart.RAM.store, e->cmd.cur_bit_addr);
        e->cmd.cur_bit_addr++;
        e->serial.sz++;
        if (e->serial.sz == 64) {
            e->mode &= ~STATE_READ;
            if (e->mode) printf("\nLeftover mode:%d", e->mode);
            e->mode = STATE_GET_CMD;
            // Finish up read...
            //pprint_RAM_hw_bit0(this, this->dma[3].op.dest_addr - (63 * 2), 64, 1);
            serial_clear(e);
        }
        return v;
    }
    return GBA_clock_current(this) >= this->cart.RAM.eeprom.ready_at;
}

