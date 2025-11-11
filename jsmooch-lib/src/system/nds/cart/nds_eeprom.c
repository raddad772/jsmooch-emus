//
// Created by . on 4/5/25.
//

#include <stdlib.h>
#include "nds_eeprom.h"
#include "../nds_bus.h"
#include "helpers/multisize_memaccess.c"

//#define flprintf(...) printf(__VA_ARGS__)
#define flprintf(...) (void)0

void NDS_eeprom_setup(NDS *this)
{
    if (this->cart.backup.detect.sz <= 512)
        this->cart.backup.detect.addr_bytes = 1;
    else if (this->cart.backup.detect.sz < 131072)
        this->cart.backup.detect.addr_bytes = 2;
    else
        this->cart.backup.detect.addr_bytes = 3;
    this->cart.backup.uh = this->cart.backup.uq = this->cart.backup.detect.sz >> 1;
    this->cart.backup.uq += this->cart.backup.uq >> 1;
    switch(this->cart.backup.detect.sz) {
        case 8192:
            this->cart.backup.page_mask = 31;
            break;
        case 32768:
            this->cart.backup.page_mask = 63;
            break;
        case 65536:
            this->cart.backup.page_mask = 127;
            break;
        case 131072:
            this->cart.backup.page_mask = 255;
            break;
    }
}

static void inc_addr(NDS *this)
{
    this->cart.backup.cmd_addr = (this->cart.backup.cmd_addr & 0xFFFFFF00) | ((this->cart.backup.cmd_addr + 1) & 0xFF);
}

static void eeprom_get_addr(NDS *this, u32 val)
{
    if (this->cart.backup.data_in_pos == 0) {
        this->cart.backup.cmd_addr = 0;
        this->cart.backup.data_in_pos = (this->cart.backup.detect.sz <= 512) ? 2 : ((this->cart.backup.detect.sz >= 131072) ? 0 : 1);
    }

    switch(this->cart.backup.data_in_pos) {
        case 0:
            this->cart.backup.cmd_addr = val << 16;
            break;
        case 1:
            this->cart.backup.cmd_addr |= (val << 8);
            break;
        case 2:
            this->cart.backup.cmd_addr |= val;
            break;
    }

    this->cart.backup.data_in_pos++;
}

static void eeprom_read(NDS *this)
{
    this->cart.backup.data_out.b8[0] = this->cart.backup.data_out.b8[1] = cR8(this->cart.backup.store->data, this->cart.backup.cmd_addr);
    flprintf("%04x: %02x", this->cart.backup.cmd_addr, this->cart.backup.data_out.b8[0]);
    this->cart.backup.cmd_addr = (this->cart.backup.cmd_addr + 1) & this->cart.backup.detect.sz_mask;
    this->cart.backup.data_in_pos++;
}

static void eeprom_write(NDS *this, u32 val)
{
    this->cart.backup.data_out.b8[0] = 0xFF;
    switch(this->cart.backup.status.write_protect_mode) {
        case 1: 
            if (this->cart.backup.cmd_addr >= this->cart.backup.uq) 
                return;
            FALLTHROUGH;
        case 2: if (this->cart.backup.cmd_addr >= this->cart.backup.uh) 
            return; 
            FALLTHROUGH;
        case 3: return;
    }
    flprintf(" %04x: %02x (en:%d)", this->cart.backup.cmd_addr, val & 0xFF, this->cart.backup.status.write_enable);

    cW8(this->cart.backup.store->data, this->cart.backup.cmd_addr, val & 0xFF);
    this->cart.backup.cmd_addr = (this->cart.backup.cmd_addr & ~this->cart.backup.page_mask) | ((this->cart.backup.cmd_addr + 1) & this->cart.backup.page_mask);


    this->cart.backup.data_in_pos++;
    this->cart.backup.store->dirty = 1;
}

static void eeprom_handle_spi_cmd(NDS *this, u32 val, u32 is_cmd)
{
    switch(this->cart.backup.cmd) {
        case 0:
            printf("\nBAD TRNASFER? %02x", val);
            break;
        case 6: // WR ENABLE
            if (is_cmd) return;
            if (val != 0) printf("\nWARN VAL %02x", val);
            if (!this->cart.backup.data_in_pos) {
                flprintf("\nWRITE ENABLE");
                this->cart.backup.status.write_enable = 1;
            }
            this->cart.backup.data_in_pos++;
            break;
        case 4: // WR DISABLE
            if (is_cmd) return;
            if (val != 0) printf("\nWARN VAL %02x", val);
            if (!this->cart.backup.data_in_pos) {
                flprintf("\nWRITE DISABLE");
                this->cart.backup.status.write_enable = 0;
            }
            this->cart.backup.data_in_pos++;
            break;
        case 5: // READ STATUS
            if (is_cmd) return;
            if (val != 0) printf("\nWARN VAL %02x", val);
            flprintf("\nFETCH STATUS (%02x)", val);

            this->cart.backup.data_out.b8[0] = this->cart.backup.status.u;
            this->cart.backup.data_out.b8[1] = this->cart.backup.status.u;
            this->cart.backup.data_out.b8[2] = this->cart.backup.status.u;
            this->cart.backup.data_out.b8[3] = this->cart.backup.status.u;
            break;
        case 1: // Write status
            if (is_cmd) return;
            if ((this->cart.backup.data_in_pos == 0) && (this->cart.backup.status.write_enable)) {
                flprintf("\nWR STATUS:%02x", val);
                this->cart.backup.status.u = val & 0b10001110;
            }
            else {
                printf("\nWRITE STATUS EXTRA!?");
            }
            this->cart.backup.data_in_pos++;
            break;
        case 3: // RDLO
            if (is_cmd) return;
            if (this->cart.backup.data_in_pos < 3) {
                flprintf("\nRDLO/ADDR %02x", val);
                eeprom_get_addr(this, val);
            }
            else {
                if (val != 0) printf("\nWARN VAL %02x", val);
                flprintf("\nRDLO/DATA ");
                eeprom_read(this);
            }
            return;
        case 0x2: // WRLO
            if (is_cmd) return;
            /*if (!this->cart.backup.status.write_enable) {
                printf("\nWRITE ENABLE QUIT!");
                return;
            }*/
            if (this->cart.backup.data_in_pos < 3) {
                flprintf("\nWRLO/ADDR %02x", val);
                eeprom_get_addr(this, val);
            }
            else {
                flprintf("\nWROLO/DATA ");
                eeprom_write(this, val);
            }
            return;

        default:
            printf("\nUnhandled eeprom SPI cmd %02x", this->cart.backup.cmd);
            break;
    }

}

void NDS_eeprom_spi_transaction(NDS *this, u32 val)
{
    u32 is_cmd = 0;
    if (!this->cart.backup.chipsel) {
        is_cmd = 1;
        flprintf("\nCMD: %02x", val);
        this->cart.backup.cmd = val;
        this->cart.backup.data_in_pos = 0;
    }
    eeprom_handle_spi_cmd(this, val, is_cmd);

    this->cart.backup.chipsel = this->cart.io.spi.next_chipsel;
    //if (!this->cart.backup.chipsel) flprintf("\nCHIPSEL DOWN...");
}
