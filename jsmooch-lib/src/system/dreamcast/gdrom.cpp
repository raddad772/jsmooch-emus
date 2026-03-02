//
// Created by RadDad772 on 3/2/24.
//

#include <cassert>
#include "dreamcast.h"
#include "gdrom.h"
#include "gdrom_reply.h"
#include "holly.h"
#include "dc_bus.h"

#define gd_printf(...)   (void)0


namespace DC::GDROM {
// LOTS of help/code from Reicast. I honestly just don't like this

void DRIVE::insert_disc(multi_file_set &mfs) {
    disc.parse_gdi(mfs);
}

void DRIVE::dma_start() {
    if (bus->g1.SB_GDST) printf("\nGDROM DMA REQUEST!");
}

void DRIVE::clear_interrupt()
{
    bus->holly.lower_interrupt(HOLLY::hirq_gdrom_cmd);
}

enum GDstatus {
    GD_BUSY = 0,
    GD_PAUSE = 1,
    GD_STANDBY = 2,
    GD_PLAY = 3,
    GD_SEEK = 4,
    GD_SCAN = 5,
    GD_OPEN = 6,
    GD_NODISC = 7,
    GD_RETRY = 8,
    GD_ERROR = 9
};

// next_state default gds_pio_end
void DRIVE::spi_pio_end(const u8* buffer, u32 len, gd_states next_state)
{
    assert(len < 0xFFFF);
    pio_buff.index = 0;
    pio_buff.size = len >> 1;
    pio_buff.next_state = next_state;

    if (buffer != nullptr){
        gd_printf("\nMEMCPY.... WORD1:%02x", *(u16 *)&buffer[0]);
        memcpy(pio_buff.data, buffer, len);
    }

    if (len == 0)
        setstate(next_state);
    else
        setstate(gds_pio_send_data);
}

DRIVE::DRIVE(core *parent) : bus(parent) {
    interrupt_reason.u = 0;
    byte_count.u = 0;
    sns_asc = sns_ascq = sns_key = 0;

    packet_cmd = (SPI_packet_cmd) {};
    pio_buff = (GDROM_PIOBUF) {.next_state=gds_waitcmd, .index=0, .size=0};
    memset(&pio_buff.data[0], 0, 65536);
    state = gds_waitcmd;
    error.u = 0;
    interrupt_reason.u = 0;
    features = 0;
    sector_number.u = 0;
    sector_count.u = 0;
    byte_count.u = 0;
    device_status.u = 0;
}

void DRIVE::setdisc() {
    cdda.playing = 0;
    sns_asc = 0x28;
    sns_ascq = 0;
    sns_key = 6;
    if (sector_number.status == GD_BUSY)
        sector_number.status = GD_PAUSE;
    else
        sector_number.status = GD_STANDBY;
    sector_number.disc_format = 8; // GDROM

}

void DRIVE::process_spi_cmd() {
    gd_printf("\nPROCESS SPI PACKET! %llu ", sh4.clock.trace_cycles);
    gd_printf("Sense: %02x %02x %02x \n", sns_asc, sns_ascq, sns_key);

    gd_printf("SPI command %02x;", packet_cmd.data_8[0]);
    gd_printf("Params: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x \n",
           packet_cmd.data_8[0], packet_cmd.data_8[1], packet_cmd.data_8[2],
           packet_cmd.data_8[3], packet_cmd.data_8[4], packet_cmd.data_8[5],
           packet_cmd.data_8[6], packet_cmd.data_8[7], packet_cmd.data_8[8],
           packet_cmd.data_8[9], packet_cmd.data_8[10], packet_cmd.data_8[11]);

    if (sns_key == 0x0 || sns_key == 0xB)
        device_status.CHECK = 0;
    else {
        //gd_printf("\nCHECK! 1...");
        device_status.CHECK = 1;
    }

    switch (packet_cmd.data_8[0]) {
        case 0: // SPI_TEST_UNIT
            gd_printf("\nSPI_TEST_UNIT");

            device_status.CHECK = sector_number.status == GD_BUSY; // Drive is ready ;)
            gd_printf("\nCHECK? %d", device_status.CHECK);

            setstate(gds_procpacketdone);
            break;
        case 0x11: // SPI_REQ_MODE
            gd_printf("SPI_REQ_MODE\n");
            spi_pio_end(reinterpret_cast<const u8 *>(&reply_11[packet_cmd.data_8[2] >> 1]), packet_cmd.data_8[4], gds_pio_end);
            break;
        case 0x13: // SPI_REQ_ERR
            gd_printf("SPI_REQ_ERR cyc:%llu\n", sh4.clock.trace_cycles);
            u8 resp[10];
            resp[0] = 0xF0;
            resp[1] = 0;
            resp[2] = sns_key;//sense
            resp[3] = 0;
            resp[4] = resp[5] = resp[6] = resp[7] = 0; //Command Specific Information
            resp[8] = sns_asc;//Additional Sense Code
            resp[9] = sns_ascq;//Additional Sense Code Qualifier

            spi_pio_end(resp, packet_cmd.data_8[4], gds_pio_end);
            sns_key = 0;
            sns_asc = 0;
            sns_ascq = 0;
            //GDStatus.CHECK=0;
            break;
        case 0x14: { // GET_TOC
            gd_printf("\nSPI GET TOC");
            u32 toc_gd[102];
            //TODO
            printf("\nFINISH GET_TOC!!!");
            //GetToc(&toc_gd[0], packet_cmd.data_8[1] & 0x1);

            spi_pio_end(reinterpret_cast<u8 *>(&toc_gd[0]), (packet_cmd.data_8[4]) | (packet_cmd.data_8[3] << 8), gds_pio_end);
            break;
        }
        case 0x70: // Reicast does this a bit weird so we do too!
            gd_printf("\nSPI CMD 70 %llu", sh4.clock.trace_cycles);
            setstate(gds_procpacketdone);
            break;
        case 0x71:
            gd_printf("\nSPI CMD 71");
            //gd_printf("SPI : unknown ? [0x71]\n");
            extern u32 reply_71_sz;

            spi_pio_end(reinterpret_cast<const u8 *>(&reply_71[0]), sizeof(reply_71), gds_pio_end);

            sector_number.status = GD_PAUSE;
            break;

        default:
            printf("\nUNKNOWN SPI COMMAND %02x", static_cast<u32>(packet_cmd.data_8[0]));
            dbg_break("UNKNOWN SPI COMMAND", bus->trace_cycles);
            break;
    }
}

void DRIVE::setstate(gd_states instate)
{
    gd_states prev_state = state;
    state = instate;
    switch(state) {
        case gds_waitcmd:
            device_status.DRDY = 1;
            device_status.BSY = 0;
            break;
        case gds_procata:
            device_status.DRDY = 0;   // Can't accept ATA command
            device_status.BSY = 1;    // Accessing command block to process command
            ATA_command();
            break;
        case gds_waitpacket:
            //assert(prev_state == gds_procata); // Validate the previous command ;)

            // Prepare for packet command
            packet_cmd.index = 0;

            // Set CoD, clear BSY and IO
            interrupt_reason.CoD = 1;
            device_status.BSY = 0;
            interrupt_reason.IO = 0;

            // Make DRQ valid
            device_status.DRQ = 1;

            // ATA can optionally raise the interrupt ...
            // RaiseInterrupt(holly_GDROM_CMD);
            break;
        case gds_procpacket:
            device_status.DRQ = 0;     // Can't accept ATA command
            device_status.BSY = 1;     // Accessing command block to process command
            process_spi_cmd();
            break;
        case gds_procpacketdone:
            device_status.DRDY = 1;
            interrupt_reason.CoD = 1;
            interrupt_reason.IO = 1;

            //Clear DRQ,BSY
            device_status.DRQ = 0;
            device_status.BSY = 0;

            //Make INTRQ valid
            bus->holly.raise_interrupt(HOLLY::hirq_gdrom_cmd, -1);

            //command finished !
            setstate(gds_waitcmd);
            break;
        case gds_pio_send_data:
        case gds_pio_get_data:
            gd_printf("\nPIO_SEND_DATA");
            //  When preparations are complete, the following steps are carried out at the device.
            //(1)   Number of bytes to be read is set in "Byte Count" register.
            byte_count.u = static_cast<u16>(pio_buff.size << 1);
            //(2)   IO bit is set and CoD bit is cleared.
            interrupt_reason.IO = 1;
            interrupt_reason.CoD = 0;
            //(3)   DRQ bit is set, BSY bit is cleared.
            device_status.DRQ = 1;
            device_status.BSY = 0;
            //(4)   INTRQ is set, and a host interrupt is issued.
            bus->holly.raise_interrupt(HOLLY::hirq_gdrom_cmd, -1);
            break;
        case gds_pio_end:
            device_status.DRQ = 0;//all data is sent !

            setstate(gds_procpacketdone);
            break;

        default:
            printf("\nGDROM UNIMPLEMENTED STATE %d", state);
            return;
    }
}

void DRIVE::reset() {
    setdisc();
    setstate(gds_waitcmd);

    device_status.NU = 0;
    device_status.BSY = 0;
    device_status.DRDY = 0; // "Response to ATA command not possible"
    device_status.DF = 0;
    device_status.DSC = 0;
    device_status.DRQ = 0;
    device_status.CORR = 0;
    device_status.CHECK = 0;
}

void DRIVE::ATA_command()
{
    error.ABORT = 0;

    if (sns_key == 0x0 || sns_key == 0xB)
        device_status.CHECK = 0;
    else {
        gd_printf("\nCHECK 1 %02x", sns_key);
        device_status.CHECK = 1;
    }

    switch(ata_cmd) {
        /*case 0x00:
            gd_printf("\nGDROM NOP!");
            device_status.BSY = 0;
            // set ABORT and ERROR in status register
            GDROM_set_interrupt();
            return;*/
        case 0x08:
            gd_printf("\nGDROM SOFT RESET!");
            reset();
            return;
        case 0xA0: // Wait for SPI packet!!!
            gd_printf("\nGDROM WAIT FOR SPI PACKET!");
            setstate(gds_waitpacket);
            return;
        case 0xEF: // Feature set
            gd_printf("\nGDROM SET FEATURES! %llu", sh4.clock.trace_cycles);

            device_status.DSC = 0;
            device_status.CHECK = 0;
            device_status.DF = 0;

            error.ABORT = 0;
            bus->holly.raise_interrupt(HOLLY::hirq_gdrom_cmd, -1);
            setstate(gds_waitcmd);
            return;
    }
    printf("\nUNKNOWN GDROM COMMAND %02x", ata_cmd);
}

void DRIVE::write(u32 addr, u64 val, u8 sz, bool* success)
{
    //printf("\nGDROM WRITE! %llu", sh4.clock.trace_cycles);
    addr &= 0x1FFFFFFF;
    //gd_printf("\nGDROM write! %08x %02x %04x %d cycle:%llu", reg | 0x5F7400, reg, val, bits, sh4.clock.trace_cycles);
    switch(addr) {
        // read / write
        case 0x005F7018: // Device control
            device_control.u = val & 2;
            gd_printf("\nGDROM SET INTERRUPT %llu", val & 2);
            return;
        case 0x005F7080: // Data
            if (state == gds_waitpacket)
            {
                gd_printf("\nGD CMD WRITE AT %llu: %04llx %d", sh4.clock.trace_cycles, val, pio_buff.index);
                packet_cmd.data_16[packet_cmd.index] = static_cast<u16>(val);
                packet_cmd.index += 1;
                if (packet_cmd.index == 6)
                    setstate(gds_procpacket);
            }
            else if (state == gds_pio_get_data){
                gd_printf("\nGD PIO GETDATA AT %llu: %04llx %d", sh4.clock.trace_cycles, val, pio_buff.index);
                pio_buff.data[pio_buff.index] = static_cast<u16>(val);
                pio_buff.index += 1;
                if (pio_buff.size == pio_buff.index)
                {
                    assert(pio_buff.next_state != gds_pio_get_data);
                    setstate(pio_buff.next_state);
                }
            }
            else
            {
                gd_printf("GDROM: Illegal Write to DATA\n");
            }
            return;
        case 0x005F7084: // Features
            gd_printf("\nGDROM SET FEATURES! %llu", sh4.clock.trace_cycles);
            features = val;
            gd_printf("\nGDROM SET FEATURES REG %02llx", val);
            return;
        case 0x005F7090: // Byte count lo
            byte_count.lo = static_cast<u8>(val);
            return;
        case 0x005F7094: // Byte count lo
            byte_count.hi = static_cast<u8>(val);
            return;
        case 0x005F7088: // Sector Count
            sector_count.u = val;
            gd_printf("\nGDROM SET SECTOR COUNT transfer_mode %d   mode_value %d", sector_count.transfer_mode, sector_count.mode_value);
            return;
        case 0x005F709C: // ATA Command
        // OOPS
            ata_cmd = val;
            setstate(gds_procata);
            return;
    }
    *success = false;
    printf("\nUnhandled GDR reg write %02x val %04llx", addr, val);
}

u64 DRIVE::read(u32 addr, u8 sz, bool* success)
{
    addr &= 0x1FFFFFFF;
    switch(addr) {
        case 0x005F7018: // AltStatus
            gd_printf("\n%02x AltSTATUS", device_status.u);
            return device_status.u | (1 << 4);
        case 0x005F709C: // Status
            gd_printf("\n%02x STATUS", device_status.u);
            clear_interrupt();
            return device_status.u | (1 << 4);
        case 0x005F7080: // DATA
            if (pio_buff.index == pio_buff.size)
            {
                gd_printf("\n-------------------------nGDROM: Illegal Read From DATA (underflow)\n");
            }
            else
            {
                u32 rv = pio_buff.data[pio_buff.index];
                pio_buff.index += 1;
                byte_count.u -= 2;
                if (pio_buff.index == pio_buff.size)
                {
                    assert(pio_buff.next_state != gds_pio_send_data);
                    //end of pio transfer !
                    setstate(pio_buff.next_state);
                }
                return rv;
            }
            return 0;
        case 0x005F708C: // Sector number
            return sector_number.u;
        case 0x005F7090: // Byte count lo
            return byte_count.lo;
        case 0x005F7094: // Byte count hi
            return byte_count.hi;

    }
    printf("\nUNKNOWN GDROM READ %8x", addr);
    *success = false;
    return 0;
}

}