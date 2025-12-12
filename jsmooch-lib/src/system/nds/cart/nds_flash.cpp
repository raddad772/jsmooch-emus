//
// Created by . on 4/5/25.
//

#include "nds_flash.h"
#include "../nds_bus.h"
#include "helpers/multisize_memaccess.cpp"

//#define flprintf(...) printf(__VA_ARGS__)
#define flprintf(...) (void)0
namespace NDS::CART {

void ridge::flash_setup()
{

}

void ridge::flash_handle_spi_cmd(u32 val)
{
    switch(backup.cmd) {
        case 0:
            printf("\nBAD FTRANSFER? %02x", val);
            break;
        case 3: // RD
            if (backup.data_in_pos < 3) {
                flprintf("\nRD/ADDR %02x", val);
                backup.data_in.b8[backup.data_in_pos] = val;
                backup.data_in_pos++;

                if (backup.data_in_pos == 3) {
#ifdef SWAPBYTES
                    backup.data_in.b8[3] = backup.data_in.b8[0];
                    backup.data_in.b8[0] = backup.data_in.b8[2];
                    backup.data_in.b8[2] = backup.data_in.b8[3];
#endif
                    backup.data_in.b8[3] = 0;
                    backup.cmd_addr = backup.data_in.u & backup.detect.sz_mask;
                    flprintf("\nRD ADDR IS:%06x", backup.cmd_addr);
                }
            }
            else {
                backup.data_out.b8[0] = backup.data_out.b8[1] =
                        cR8(backup.store->data, backup.cmd_addr);
                flprintf("\nRD/DATA %06x:%02x", backup.cmd_addr, backup.data_out.b8[0]);
                backup.cmd_addr = (backup.cmd_addr + 1) & backup.detect.sz_mask;
                backup.data_in_pos++;
            }
            return;
        case 0xA:
            if (backup.data_in_pos < 3) {
                flprintf("\nWR/ADDR %02x", val);
                backup.data_in.b8[backup.data_in_pos] = val;
                backup.data_in_pos++;

                if (backup.data_in_pos == 3) {
#ifdef SWAPBYTES
                    backup.data_in.b8[3] = backup.data_in.b8[0];
                    backup.data_in.b8[0] = backup.data_in.b8[2];
                    backup.data_in.b8[2] = backup.data_in.b8[3];
#endif
                    backup.data_in.b8[3] = 0;
                    backup.cmd_addr = backup.data_in.u & backup.detect.sz_mask;
                    flprintf("\nWR ADDR IS:%06x", backup.cmd_addr);
                }
            }
            else {
                cW8(backup.store->data, backup.cmd_addr, val & 0xFF);
                flprintf("\nRD/DATA %06x:%02x", backup.cmd_addr, val & 0xFF);
                inc_addr();
                backup.store->dirty = true;
                backup.data_in_pos++;
            }
            return;

        case 6:
            backup.status.write_enable = 1;
            break;
        case 4:
            backup.status.write_enable = 0;
            break;
        case 5:
            //printf("\nRD STATUS REG!");
            backup.data_out.b8[0] = backup.status.u;
            backup.data_out.b8[1] = backup.status.u;
            backup.data_out.b8[2] = backup.status.u;
            backup.data_out.b8[3] = backup.status.u;
            break;
        default: {
            static int a = 1;
            if (a) {
                printf("\nUnhandled flash SPI cmd(s) %02x", backup.cmd);
                a = 0;
            }
            break; }
    }
}


void ridge::flash_spi_transaction(u32 val)
{
    if (!backup.chipsel) {
        flprintf("\nCMD: %02x", val);
        backup.cmd = val;
        backup.data_in_pos = 0;
    }
    else {
        flash_handle_spi_cmd(val);
    }

    backup.chipsel = io.spi.next_chipsel;
}

}