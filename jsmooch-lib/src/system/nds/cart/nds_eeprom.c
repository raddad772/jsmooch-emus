//
// Created by . on 4/5/25.
//

#include <stdlib.h>
#include "nds_eeprom.h"
#include "../nds_bus.h"
#include "helpers/multisize_memaccess.c"

#define flprintf(...) printf(__VA_ARGS__)

static void inc_addr(struct NDS *this)
{
    this->cart.RAM.cmd_addr = (this->cart.RAM.cmd_addr & 0xFFFFFF00) | ((this->cart.RAM.cmd_addr + 1) & 0xFF);
}

static void eeprom_get_addr(struct NDS *this, u32 val)
{
    this->cart.RAM.data_in.b8[this->cart.RAM.data_in_pos] = val;
    this->cart.RAM.data_in_pos++;

    if (this->cart.RAM.data_in_pos == this->cart.RAM.detect.addr_bytes) {
        this->cart.RAM.data_in.b8[3] = 0;
        switch(this->cart.RAM.detect.addr_bytes) {
            case 1:
                this->cart.RAM.cmd_addr = this->cart.RAM.data_in.b8[0];
                //printf("\nEEPROM r/w addr1: %02x", this->cart.RAM.cmd_addr);
                return;
            case 2:
                this->cart.RAM.data_in.b8[2] = 0;
                this->cart.RAM.cmd_addr = bswap_16(this->cart.RAM.data_in.u) & this->cart.RAM.detect.sz_mask;
                //printf("\nEEPROM r/w addr2: %04x", this->cart.RAM.cmd_addr);
                return;
            case 3:
                this->cart.RAM.data_in.b8[3] = this->cart.RAM.data_in.b8[0];
                this->cart.RAM.data_in.b8[0] = this->cart.RAM.data_in.b8[2];
                this->cart.RAM.data_in.b8[2] = this->cart.RAM.data_in.b8[3];
                this->cart.RAM.cmd_addr = this->cart.RAM.data_in.u & this->cart.RAM.detect.sz_mask;
                //printf("\nEEPROM r/w addr3: %06x", this->cart.RAM.cmd_addr);
                return;
            default:
                NOGOHERE;
        }
    }
}

static void eeprom_read(struct NDS *this)
{
    this->cart.RAM.data_out.b8[0] = this->cart.RAM.data_out.b8[1] = cR8(this->cart.RAM.store->data, this->cart.RAM.cmd_addr);
    //printf(" %04x: %02x", this->cart.RAM.cmd_addr, this->cart.RAM.data_out.b8[0]);
    inc_addr(this);
    this->cart.RAM.data_in_pos++;
}

static void eeprom_write(struct NDS *this, u32 val)
{
    //printf(" %04x: %02x (en:%d)", this->cart.RAM.cmd_addr, val & 0xFF, this->cart.RAM.status.write_enable);
    //if (this->cart.RAM.status.write_enable)
    cW8(this->cart.RAM.store->data, this->cart.RAM.cmd_addr, val & 0xFF);
    //printf("\nIMMEDIATE RE_READ:%02x", (u32)cR8(this->cart.RAM.store->data, this->cart.RAM.cmd_addr));
    inc_addr(this);
    this->cart.RAM.data_in_pos++;
    this->cart.RAM.store->dirty = 1;
}

static void eeprom_handle_spi_cmd(struct NDS *this, u32 val)
{
    switch(this->cart.RAM.cmd) {
        case 0:
            printf("\nBAD TRNASFER? %02x", val);
            break;
        case 6:
            //printf("\nWRITE ENABLE");
            if (val != 0) printf("\nWARN VAL %02x", val);
            this->cart.RAM.status.write_enable = 1;
            break;
        case 4:
            //printf("\nWRITE DISABLE");
            if (val != 0) printf("\nWARN VAL %02x", val);
            this->cart.RAM.status.write_enable = 0;
            break;
        case 5:
            //printf("\nFETCH STATUS (%02x)", val);
            if (val != 0) printf("\nWARN VAL %02x", val);
            this->cart.RAM.data_out.b8[0] = this->cart.RAM.status.u;
            this->cart.RAM.data_out.b8[1] = this->cart.RAM.status.u;
            this->cart.RAM.data_out.b8[2] = this->cart.RAM.status.u;
            this->cart.RAM.data_out.b8[3] = this->cart.RAM.status.u;
            break;

        case 3: // RDLO
            if (this->cart.RAM.data_in_pos < (this->cart.RAM.detect.addr_bytes)) {
                //printf("\nRDLO/ADDR %02x", val);
                eeprom_get_addr(this, val);
            }
            else {
                //printf("\nRDLO/DATA ");
                //if (val != 0) printf("\nWARN VAL %02x", val);
                eeprom_read(this);
            }
            return;
        case 0xB: // RDHI
            //printf("\nRDHI");
            if (this->cart.RAM.detect.addr_bytes > 1) {
                printf("\nWHAT!? >1");
                return;
            }

            if (this->cart.RAM.data_in_pos == 0) {
                this->cart.RAM.cmd_addr = 0x100 + (val & 0xFF);
                this->cart.RAM.data_in_pos++;
            }
            else {
                eeprom_read(this);
            }
            return;
        case 0xA: // WRHI
            //printf("\nWRHI");
            if (this->cart.RAM.detect.addr_bytes > 1) {
                //printf("\nWHAT!? >1_2");
                return;
            }

            if (this->cart.RAM.data_in_pos == 0) {
                this->cart.RAM.cmd_addr = 0x100 + (val & 0xFF);
                this->cart.RAM.data_in_pos++;
            }
            else {
                eeprom_write(this, val);
            }
            return;
        case 0x2: // WRLO
            if (this->cart.RAM.data_in_pos < (this->cart.RAM.detect.addr_bytes)) {
                //printf("\nWRLO/ADDR %02x", val);
                eeprom_get_addr(this, val);
            }
            else {
                //printf("\nWROLO/DATA ");
                eeprom_write(this, val);
            }
            return;

        default:
            printf("\nUnhandled eeprom SPI cmd %02x", this->cart.RAM.cmd);
            break;
    }

}

void NDS_eeprom_spi_transaction(struct NDS *this, u32 val)
{
    if (!this->cart.RAM.chipsel) {
        //printf("\nCMD: %02x", val);
        this->cart.RAM.cmd = val;
        this->cart.RAM.data_in_pos = 0;
    }
    else {
        eeprom_handle_spi_cmd(this, val);
    }

    this->cart.RAM.chipsel = this->cart.io.spi.next_chipsel;
    if (!this->cart.RAM.chipsel) printf("\nCHIPSEL DOWN...");
}
