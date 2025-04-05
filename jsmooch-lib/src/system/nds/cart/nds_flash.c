//
// Created by . on 4/5/25.
//

#include <stdlib.h>
#include "nds_flash.h"
#include "../nds_bus.h"
#include "helpers/multisize_memaccess.c"

#define flprintf(...) printf(__VA_ARGS__)

static void inc_addr(struct NDS *this)
{
    this->cart.RAM.cmd_addr = (this->cart.RAM.cmd_addr & 0xFFFFFF00) | ((this->cart.RAM.cmd_addr + 1) & 0xFF);
}

static void flash_handle_spi_cmd(struct NDS *this, u32 val)
{
    switch(this->cart.RAM.cmd) {
        case 0:
            printf("\nBAD FTRANSFER? %02x", val);
            break;
        case 3: // RD
            if (this->cart.RAM.data_in_pos < 3) {
                flprintf("\nRD/ADDR %02x", val);
                this->cart.RAM.data_in.b8[this->cart.RAM.data_in_pos] = val;
                this->cart.RAM.data_in_pos++;

                if (this->cart.RAM.data_in_pos == 3) {
                    this->cart.RAM.data_in.b8[3] = this->cart.RAM.data_in.b8[0];
                    this->cart.RAM.data_in.b8[0] = this->cart.RAM.data_in.b8[2];
                    this->cart.RAM.data_in.b8[2] = this->cart.RAM.data_in.b8[3];
                    this->cart.RAM.data_in.b8[3] = 0;
                    this->cart.RAM.cmd_addr = this->cart.RAM.data_in.u & this->cart.RAM.detect.sz_mask;
                    flprintf("\nRD ADDR IS:%06x", this->cart.RAM.cmd_addr);
                }
            }
            else {
                this->cart.RAM.data_out.b8[0] = this->cart.RAM.data_out.b8[1] =
                        cR8(this->cart.RAM.store->data, this->cart.RAM.cmd_addr);
                flprintf("\nRD/DATA %06x:%02x", this->cart.RAM.cmd_addr, this->cart.RAM.data_out.b8[0]);
                this->cart.RAM.cmd_addr = (this->cart.RAM.cmd_addr + 1) & this->cart.RAM.detect.sz_mask;
                this->cart.RAM.data_in_pos++;
            }
            return;
        case 0xA:
            if (this->cart.RAM.data_in_pos < 3) {
                flprintf("\nWR/ADDR %02x", val);
                this->cart.RAM.data_in.b8[this->cart.RAM.data_in_pos] = val;
                this->cart.RAM.data_in_pos++;

                if (this->cart.RAM.data_in_pos == 3) {
                    this->cart.RAM.data_in.b8[3] = this->cart.RAM.data_in.b8[0];
                    this->cart.RAM.data_in.b8[0] = this->cart.RAM.data_in.b8[2];
                    this->cart.RAM.data_in.b8[2] = this->cart.RAM.data_in.b8[3];
                    this->cart.RAM.data_in.b8[3] = 0;
                    this->cart.RAM.cmd_addr = this->cart.RAM.data_in.u & this->cart.RAM.detect.sz_mask;
                    flprintf("\nWR ADDR IS:%06x", this->cart.RAM.cmd_addr);
                }
            }
            else {
                cW8(this->cart.RAM.store->data, this->cart.RAM.cmd_addr, val & 0xFF);
                flprintf("\nRD/DATA %06x:%02x", this->cart.RAM.cmd_addr, val & 0xFF);
                inc_addr(this);
                this->cart.RAM.store->dirty = 1;
                this->cart.RAM.data_in_pos++;
            }
            return;

        case 6:
            this->cart.RAM.status.write_enable = 1;
            break;
        case 4:
            this->cart.RAM.status.write_enable = 0;
            break;
        case 5:
            //printf("\nRD STATUS REG!");
            this->cart.RAM.data_out.b8[0] = this->cart.RAM.status.u;
            this->cart.RAM.data_out.b8[1] = this->cart.RAM.status.u;
            this->cart.RAM.data_out.b8[2] = this->cart.RAM.status.u;
            this->cart.RAM.data_out.b8[3] = this->cart.RAM.status.u;
            break;
        default: {
            static int a = 1;
            if (a) {
                printf("\nUnhandled flash SPI cmd(s) %02x", this->cart.RAM.cmd);
                a = 0;
            }
            break; }
    }
}


void NDS_flash_spi_transaction(struct NDS *this, u32 val)
{
    if (!this->cart.RAM.chipsel) {
        flprintf("\nCMD: %02x", val);
        this->cart.RAM.cmd = val;
        this->cart.RAM.data_in_pos = 0;
    }
    else {
        flash_handle_spi_cmd(this, val);
    }

    this->cart.RAM.chipsel = this->cart.io.spi.next_chipsel;
}

