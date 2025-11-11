//
// Created by . on 4/5/25.
//

#include <stdlib.h>
#include "nds_flash.h"
#include "../nds_bus.h"
#include "helpers/multisize_memaccess.c"

//#define flprintf(...) printf(__VA_ARGS__)
#define flprintf(...) (void)0

void NDS_flash_setup(NDS *this)
{

}

static void inc_addr(NDS *this)
{
    this->cart.backup.cmd_addr = (this->cart.backup.cmd_addr & 0xFFFFFF00) | ((this->cart.backup.cmd_addr + 1) & 0xFF);
}

static void flash_handle_spi_cmd(NDS *this, u32 val)
{
    switch(this->cart.backup.cmd) {
        case 0:
            printf("\nBAD FTRANSFER? %02x", val);
            break;
        case 3: // RD
            if (this->cart.backup.data_in_pos < 3) {
                flprintf("\nRD/ADDR %02x", val);
                this->cart.backup.data_in.b8[this->cart.backup.data_in_pos] = val;
                this->cart.backup.data_in_pos++;

                if (this->cart.backup.data_in_pos == 3) {
#ifdef SWAPBYTES
                    this->cart.backup.data_in.b8[3] = this->cart.backup.data_in.b8[0];
                    this->cart.backup.data_in.b8[0] = this->cart.backup.data_in.b8[2];
                    this->cart.backup.data_in.b8[2] = this->cart.backup.data_in.b8[3];
#endif
                    this->cart.backup.data_in.b8[3] = 0;
                    this->cart.backup.cmd_addr = this->cart.backup.data_in.u & this->cart.backup.detect.sz_mask;
                    flprintf("\nRD ADDR IS:%06x", this->cart.backup.cmd_addr);
                }
            }
            else {
                this->cart.backup.data_out.b8[0] = this->cart.backup.data_out.b8[1] =
                        cR8(this->cart.backup.store->data, this->cart.backup.cmd_addr);
                flprintf("\nRD/DATA %06x:%02x", this->cart.backup.cmd_addr, this->cart.backup.data_out.b8[0]);
                this->cart.backup.cmd_addr = (this->cart.backup.cmd_addr + 1) & this->cart.backup.detect.sz_mask;
                this->cart.backup.data_in_pos++;
            }
            return;
        case 0xA:
            if (this->cart.backup.data_in_pos < 3) {
                flprintf("\nWR/ADDR %02x", val);
                this->cart.backup.data_in.b8[this->cart.backup.data_in_pos] = val;
                this->cart.backup.data_in_pos++;

                if (this->cart.backup.data_in_pos == 3) {
#ifdef SWAPBYTES
                    this->cart.backup.data_in.b8[3] = this->cart.backup.data_in.b8[0];
                    this->cart.backup.data_in.b8[0] = this->cart.backup.data_in.b8[2];
                    this->cart.backup.data_in.b8[2] = this->cart.backup.data_in.b8[3];
#endif
                    this->cart.backup.data_in.b8[3] = 0;
                    this->cart.backup.cmd_addr = this->cart.backup.data_in.u & this->cart.backup.detect.sz_mask;
                    flprintf("\nWR ADDR IS:%06x", this->cart.backup.cmd_addr);
                }
            }
            else {
                cW8(this->cart.backup.store->data, this->cart.backup.cmd_addr, val & 0xFF);
                flprintf("\nRD/DATA %06x:%02x", this->cart.backup.cmd_addr, val & 0xFF);
                inc_addr(this);
                this->cart.backup.store->dirty = 1;
                this->cart.backup.data_in_pos++;
            }
            return;

        case 6:
            this->cart.backup.status.write_enable = 1;
            break;
        case 4:
            this->cart.backup.status.write_enable = 0;
            break;
        case 5:
            //printf("\nRD STATUS REG!");
            this->cart.backup.data_out.b8[0] = this->cart.backup.status.u;
            this->cart.backup.data_out.b8[1] = this->cart.backup.status.u;
            this->cart.backup.data_out.b8[2] = this->cart.backup.status.u;
            this->cart.backup.data_out.b8[3] = this->cart.backup.status.u;
            break;
        default: {
            static int a = 1;
            if (a) {
                printf("\nUnhandled flash SPI cmd(s) %02x", this->cart.backup.cmd);
                a = 0;
            }
            break; }
    }
}


void NDS_flash_spi_transaction(NDS *this, u32 val)
{
    if (!this->cart.backup.chipsel) {
        flprintf("\nCMD: %02x", val);
        this->cart.backup.cmd = val;
        this->cart.backup.data_in_pos = 0;
    }
    else {
        flash_handle_spi_cmd(this, val);
    }

    this->cart.backup.chipsel = this->cart.io.spi.next_chipsel;
}

