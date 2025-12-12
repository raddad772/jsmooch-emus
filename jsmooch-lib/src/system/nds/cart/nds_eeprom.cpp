//
// Created by . on 4/5/25.
//

#include <cstdlib>
#include "nds_eeprom.h"
#include "../nds_bus.h"
#include "helpers/multisize_memaccess.cpp"

//#define flprintf(...) printf(__VA_ARGS__)
#define flprintf(...) (void)0

namespace NDS::CART{

void ridge::eeprom_setup()
{
    if (backup.detect.sz <= 512)
        backup.detect.addr_bytes = 1;
    else if (backup.detect.sz < 131072)
        backup.detect.addr_bytes = 2;
    else
        backup.detect.addr_bytes = 3;
    backup.uh = backup.uq = backup.detect.sz >> 1;
    backup.uq += backup.uq >> 1;
    switch(backup.detect.sz) {
        case 8192:
            backup.page_mask = 31;
            break;
        case 32768:
            backup.page_mask = 63;
            break;
        case 65536:
            backup.page_mask = 127;
            break;
        case 131072:
            backup.page_mask = 255;
            break;
    }
}

void ridge::eeprom_get_addr(u32 val)
{
    if (backup.data_in_pos == 0) {
        backup.cmd_addr = 0;
        backup.data_in_pos = (backup.detect.sz <= 512) ? 2 : ((backup.detect.sz >= 131072) ? 0 : 1);
    }

    switch(backup.data_in_pos) {
        case 0:
            backup.cmd_addr = val << 16;
            break;
        case 1:
            backup.cmd_addr |= (val << 8);
            break;
        case 2:
            backup.cmd_addr |= val;
            break;
    }

    backup.data_in_pos++;
}

void ridge::eeprom_read()
{
    backup.data_out.b8[0] = backup.data_out.b8[1] = cR8(backup.store->data, backup.cmd_addr);
    flprintf("%04x: %02x", backup.cmd_addr, backup.data_out.b8[0]);
    backup.cmd_addr = (backup.cmd_addr + 1) & backup.detect.sz_mask;
    backup.data_in_pos++;
}

void ridge::eeprom_write(u32 val)
{
    backup.data_out.b8[0] = 0xFF;
    switch(backup.status.write_protect_mode) {
        case 1: 
            if (backup.cmd_addr >= backup.uq) 
                return;
            FALLTHROUGH;
        case 2: if (backup.cmd_addr >= backup.uh) 
            return; 
            FALLTHROUGH;
        case 3: return;
    }
    flprintf(" %04x: %02x (en:%d)", backup.cmd_addr, val & 0xFF, backup.status.write_enable);

    cW8(backup.store->data, backup.cmd_addr, val & 0xFF);
    backup.cmd_addr = (backup.cmd_addr & ~backup.page_mask) | ((backup.cmd_addr + 1) & backup.page_mask);


    backup.data_in_pos++;
    backup.store->dirty = true;
}

void ridge::eeprom_handle_spi_cmd(u32 val, u32 is_cmd)
{
    switch(backup.cmd) {
        case 0:
            printf("\nBAD TRNASFER? %02x", val);
            break;
        case 6: // WR ENABLE
            if (is_cmd) return;
            if (val != 0) printf("\nWARN VAL %02x", val);
            if (!backup.data_in_pos) {
                flprintf("\nWRITE ENABLE");
                backup.status.write_enable = 1;
            }
            backup.data_in_pos++;
            break;
        case 4: // WR DISABLE
            if (is_cmd) return;
            if (val != 0) printf("\nWARN VAL %02x", val);
            if (!backup.data_in_pos) {
                flprintf("\nWRITE DISABLE");
                backup.status.write_enable = 0;
            }
            backup.data_in_pos++;
            break;
        case 5: // READ STATUS
            if (is_cmd) return;
            if (val != 0) printf("\nWARN VAL %02x", val);
            flprintf("\nFETCH STATUS (%02x)", val);

            backup.data_out.b8[0] = backup.status.u;
            backup.data_out.b8[1] = backup.status.u;
            backup.data_out.b8[2] = backup.status.u;
            backup.data_out.b8[3] = backup.status.u;
            break;
        case 1: // Write status
            if (is_cmd) return;
            if ((backup.data_in_pos == 0) && (backup.status.write_enable)) {
                flprintf("\nWR STATUS:%02x", val);
                backup.status.u = val & 0b10001110;
            }
            else {
                printf("\nWRITE STATUS EXTRA!?");
            }
            backup.data_in_pos++;
            break;
        case 3: // RDLO
            if (is_cmd) return;
            if (backup.data_in_pos < 3) {
                flprintf("\nRDLO/ADDR %02x", val);
                eeprom_get_addr(val);
            }
            else {
                if (val != 0) printf("\nWARN VAL %02x", val);
                flprintf("\nRDLO/DATA ");
                eeprom_read();
            }
            return;
        case 0x2: // WRLO
            if (is_cmd) return;
            /*if (!backup.status.write_enable) {
                printf("\nWRITE ENABLE QUIT!");
                return;
            }*/
            if (backup.data_in_pos < 3) {
                flprintf("\nWRLO/ADDR %02x", val);
                eeprom_get_addr(val);
            }
            else {
                flprintf("\nWROLO/DATA ");
                eeprom_write(val);
            }
            return;

        default:
            printf("\nUnhandled eeprom SPI cmd %02x", backup.cmd);
            break;
    }

}

void ridge::eeprom_spi_transaction(u32 val)
{
    u32 is_cmd = 0;
    if (!backup.chipsel) {
        is_cmd = 1;
        flprintf("\nCMD: %02x", val);
        backup.cmd = val;
        backup.data_in_pos = 0;
    }
    eeprom_handle_spi_cmd(val, is_cmd);

    backup.chipsel = io.spi.next_chipsel;
    //if (!backup.chipsel) flprintf("\nCHIPSEL DOWN...");
}
}