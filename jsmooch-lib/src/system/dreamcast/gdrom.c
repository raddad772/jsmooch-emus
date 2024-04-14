//
// Created by RadDad772 on 3/2/24.
//

#include "string.h"
#include "assert.h"
#include "stdio.h"
#include "dreamcast.h"
#include "gdrom.h"
#include "holly.h"
#include "gdrom_reply.h"

#define gd_printf(...)   (void)0

static void GDROM_setstate(struct DC* this, enum gd_states state);
static void GDROM_ATA_command(struct DC* this);

// LOTS of help/code from Reicast. I honestly just don't like this

static void gdrom_clear_interrupt(struct DC* this)
{
    holly_lower_interrupt(this, hirq_gdrom_cmd);
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
static void GDROM_spi_pio_end(struct DC* this, u8* buffer, u32 len, enum gd_states next_state)
{
    assert(len < 0xFFFF);
    this->gdrom.pio_buff.index = 0;
    this->gdrom.pio_buff.size = len >> 1;
    this->gdrom.pio_buff.next_state = next_state;

    if (buffer != NULL){
        gd_printf("\nMEMCPY.... WORD1:%02x", *(u16 *)&buffer[0]);
        memcpy(this->gdrom.pio_buff.data, buffer, len);
    }

    if (len == 0)
        GDROM_setstate(this, next_state);
    else
        GDROM_setstate(this, gds_pio_send_data);
}

void GDROM_init(struct DC* this) {
    this->gdrom.interrupt_reason.u = 0;
    this->gdrom.byte_count.u = 0;
    this->gdrom.sns_asc = this->gdrom.sns_ascq = this->gdrom.sns_key = 0;

    this->gdrom.packet_cmd = (struct SPI_packet_cmd) {};
    this->gdrom.pio_buff = (struct GDROM_PIOBUF) {.next_state=gds_waitcmd, .index=0, .size=0};
    memset(&this->gdrom.pio_buff.data[0], 0, 65536);
    this->gdrom.state = gds_waitcmd;
    this->gdrom.error.u = 0;
    this->gdrom.interrupt_reason.u = 0;
    this->gdrom.features = 0;
    this->gdrom.sector_number.u = 0;
    this->gdrom.sector_count.u = 0;
    this->gdrom.byte_count.u = 0;
    this->gdrom.device_status.u = 0;
}

static void GDROM_setdisc(struct DC* this) {
    this->gdrom.cdda.playing = 0;
    this->gdrom.sns_asc = 0x28;
    this->gdrom.sns_ascq = 0;
    this->gdrom.sns_key = 6;
    if (this->gdrom.sector_number.status == GD_BUSY)
        this->gdrom.sector_number.status = GD_PAUSE;
    else
        this->gdrom.sector_number.status = GD_STANDBY;
    this->gdrom.sector_number.disc_format = 8; // GDROM

}

static void GDROM_process_spi_cmd(struct DC* this) {
    gd_printf("\nPROCESS SPI PACKET! %llu ", this->sh4.trace_cycles);
    gd_printf("Sense: %02x %02x %02x \n", this->gdrom.sns_asc, this->gdrom.sns_ascq, this->gdrom.sns_key);

    gd_printf("SPI command %02x;", this->gdrom.packet_cmd.data_8[0]);
    gd_printf("Params: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x \n",
           this->gdrom.packet_cmd.data_8[0], this->gdrom.packet_cmd.data_8[1], this->gdrom.packet_cmd.data_8[2],
           this->gdrom.packet_cmd.data_8[3], this->gdrom.packet_cmd.data_8[4], this->gdrom.packet_cmd.data_8[5],
           this->gdrom.packet_cmd.data_8[6], this->gdrom.packet_cmd.data_8[7], this->gdrom.packet_cmd.data_8[8],
           this->gdrom.packet_cmd.data_8[9], this->gdrom.packet_cmd.data_8[10], this->gdrom.packet_cmd.data_8[11]);

    //if (this->gdrom.sns_key == 0x0 || this->gdrom.sns_key == 0xB)
        //this->gdrom.device_status.CHECK = 0;
    //else {
        //gd_printf("\nCHECK! 1...");
        //this->gdrom.device_status.CHECK = 1;
    //}

    switch (this->gdrom.packet_cmd.data_8[0]) {
        case 0: // SPI_TEST_UNIT
            gd_printf("\nSPI_TEST_UNIT");

            //this->gdrom.device_status.CHECK = this->gdrom.sector_number.status == GD_BUSY; // Drive is ready ;)
            gd_printf("\nCHECK? %d", this->gdrom.device_status.CHECK);

            GDROM_setstate(this, gds_procpacketdone);
            break;
        case 0x11: // SPI_REQ_MODE
            gd_printf("SPI_REQ_MODE\n");
            GDROM_spi_pio_end(this, (u8*)&reply_11[this->gdrom.packet_cmd.data_8[2] >> 1], this->gdrom.packet_cmd.data_8[4], gds_pio_end);
            break;
        case 0x13: // SPI_REQ_ERR
            gd_printf("SPI_REQ_ERR cyc:%llu\n", this->sh4.trace_cycles);
            u8 resp[10];
            resp[0] = 0xF0;
            resp[1] = 0;
            resp[2] = this->gdrom.sns_key;//sense
            resp[3] = 0;
            resp[4] = resp[5] = resp[6] = resp[7] = 0; //Command Specific Information
            resp[8] = this->gdrom.sns_asc;//Additional Sense Code
            resp[9] = this->gdrom.sns_ascq;//Additional Sense Code Qualifier

            GDROM_spi_pio_end(this, resp, this->gdrom.packet_cmd.data_8[4], gds_pio_end);
            this->gdrom.sns_key = 0;
            this->gdrom.sns_asc = 0;
            this->gdrom.sns_ascq = 0;
            //GDStatus.CHECK=0;
            break;
        case 0x14: { // GET_TOC
            gd_printf("\nSPI GET TOC");
            u32 toc_gd[102];
            //TODO
            GDI_GetToc(&this->gdrom.gdi, &toc_gd[0], this->gdrom.packet_cmd.data_8[1] & 0x1);

            GDROM_spi_pio_end(this, (u8*)&toc_gd[0], (this->gdrom.packet_cmd.data_8[4]) | (this->gdrom.packet_cmd.data_8[3] << 8), gds_pio_end);

        }
        case 0x70: // Reicast does this a bit weird so we do too!
            gd_printf("\nSPI CMD 70 %llu", this->sh4.trace_cycles);
            GDROM_setstate(this, gds_procpacketdone);
            break;
        case 0x71:
            gd_printf("\nSPI CMD 71");
            //gd_printf("SPI : unknown ? [0x71]\n");
            extern u32 reply_71_sz;

            GDROM_spi_pio_end(this, (u8*)&reply_71[0], sizeof(reply_71), gds_pio_end);

            this->gdrom.sector_number.status = GD_PAUSE;
            break;

        default:
            printf("\nUNKNOWN SPI COMMAND %02x", (u32)this->gdrom.packet_cmd.data_8[0]);
            dbg_break();
            break;
    }
}

static void GDROM_setstate(struct DC* this, enum gd_states state)
{
    enum gd_states prev_state = this->gdrom.state;
    this->gdrom.state = state;
    switch(state) {
        case gds_waitcmd:
            this->gdrom.device_status.DRDY = 1;
            this->gdrom.device_status.BSY = 0;
            break;
        case gds_procata:
            this->gdrom.device_status.DRDY = 0;   // Can't accept ATA command
            this->gdrom.device_status.BSY = 1;    // Accessing command block to process command
            GDROM_ATA_command(this);
            break;
        case gds_waitpacket:
            //assert(prev_state == gds_procata); // Validate the previous command ;)

            // Prepare for packet command
            this->gdrom.packet_cmd.index = 0;

            // Set CoD, clear BSY and IO
            this->gdrom.interrupt_reason.CoD = 1;
            this->gdrom.device_status.BSY = 0;
            this->gdrom.interrupt_reason.IO = 0;

            // Make DRQ valid
            this->gdrom.device_status.DRQ = 1;

            // ATA can optionally raise the interrupt ...
            // RaiseInterrupt(holly_GDROM_CMD);
            break;
        case gds_procpacket:
            this->gdrom.device_status.DRQ = 0;     // Can't accept ATA command
            this->gdrom.device_status.BSY = 1;     // Accessing command block to process command
            GDROM_process_spi_cmd(this);
            break;
        case gds_procpacketdone:
            this->gdrom.device_status.DRDY = 1;
            this->gdrom.interrupt_reason.CoD = 1;
            this->gdrom.interrupt_reason.IO = 1;

            //Clear DRQ,BSY
            this->gdrom.device_status.DRQ = 0;
            this->gdrom.device_status.BSY = 0;

            //Make INTRQ valid
            holly_raise_interrupt(this, hirq_gdrom_cmd, -1);

            //command finished !
            GDROM_setstate(this, gds_waitcmd);
            break;
        case gds_pio_send_data:
        case gds_pio_get_data:
            gd_printf("\nPIO_SEND_DATA");
            //  When preparations are complete, the following steps are carried out at the device.
            //(1)   Number of bytes to be read is set in "Byte Count" register.
            this->gdrom.byte_count.u = (u16)(this->gdrom.pio_buff.size << 1);
            //(2)   IO bit is set and CoD bit is cleared.
            this->gdrom.interrupt_reason.IO = 1;
            this->gdrom.interrupt_reason.CoD = 0;
            //(3)   DRQ bit is set, BSY bit is cleared.
            this->gdrom.device_status.DRQ = 1;
            this->gdrom.device_status.BSY = 0;
            //(4)   INTRQ is set, and a host interrupt is issued.
            holly_raise_interrupt(this, hirq_gdrom_cmd, -1);
            break;
        case gds_pio_end:

            this->gdrom.device_status.DRQ = 0;//all data is sent !

            GDROM_setstate(this, gds_procpacketdone);
            break;

        default:
            printf("\nGDROM UNIMPLEMENTED STATE %d", state);
            return;
    }
}

void GDROM_reset(struct DC* this) {
    GDROM_setdisc(this);
    GDROM_setstate(this, gds_waitcmd);

    this->gdrom.device_status.NU = 0;
    this->gdrom.device_status.BSY = 0;
    this->gdrom.device_status.DRDY = 0; // "Response to ATA command not possible"
    this->gdrom.device_status.DF = 0;
    this->gdrom.device_status.DSC = 0;
    this->gdrom.device_status.DRQ = 0;
    this->gdrom.device_status.CORR = 0;
    this->gdrom.device_status.CHECK = 0;
}

static void GDROM_ATA_command(struct DC* this)
{
    this->gdrom.error.ABORT = 0;

    if (this->gdrom.sns_key == 0x0 || this->gdrom.sns_key == 0xB)
        this->gdrom.device_status.CHECK = 0;
    else {
        gd_printf("\nCHECK 1 %02x", this->gdrom.sns_key);
        //this->gdrom.device_status.CHECK = 1;
    }

    switch(this->gdrom.ata_cmd) {
        /*case 0x00:
            gd_printf("\nGDROM NOP!");
            this->gdrom.device_status.BSY = 0;
            // set ABORT and ERROR in status register
            GDROM_set_interrupt(this);
            return;*/
        case 0x08:
            gd_printf("\nGDROM SOFT RESET!");
            GDROM_reset(this);
            return;
        case 0xA0: // Wait for SPI packet!!!
            gd_printf("\nGDROM WAIT FOR SPI PACKET!");
            GDROM_setstate(this, gds_waitpacket);
            return;
        case 0xEF: // Feature set
            gd_printf("\nGDROM SET FEATURES! %llu", this->sh4.trace_cycles);

            this->gdrom.device_status.DSC = 0;
            this->gdrom.device_status.CHECK = 0;
            this->gdrom.device_status.DF = 0;

            this->gdrom.error.ABORT = 0;
            holly_raise_interrupt(this, hirq_gdrom_cmd, -1);
            GDROM_setstate(this, gds_waitcmd);
            return;
    }
    printf("\nUNKNOWN GDROM COMMAND %02x", this->gdrom.ata_cmd);
}

void GDROM_write(struct DC* this, u32 addr, u64 val, u32 sz, u32* success)
{
    addr &= 0x1FFFFFFF;
    //gd_printf("\nGDROM write! %08x %02x %04x %d cycle:%llu", reg | 0x5F7400, reg, val, bits, this->sh4.trace_cycles);
    switch(addr) {
        // read / write
        case 0x005F7018: // Device control
            this->gdrom.device_control.u = val & 2;
            gd_printf("\nGDROM SET INTERRUPT %llu", val & 2);
            return;
        case 0x005F7080: // Data
            if (this->gdrom.state == gds_waitpacket)
            {
                gd_printf("\nGD CMD WRITE AT %llu: %04llx %d", this->sh4.trace_cycles, val, this->gdrom.pio_buff.index);
                this->gdrom.packet_cmd.data_16[this->gdrom.packet_cmd.index] = (u16)val;
                this->gdrom.packet_cmd.index += 1;
                if (this->gdrom.packet_cmd.index == 6)
                    GDROM_setstate(this, gds_procpacket);
            }
            else if (this->gdrom.state == gds_pio_get_data){
                gd_printf("\nGD PIO GETDATA AT %llu: %04llx %d", this->sh4.trace_cycles, val, this->gdrom.pio_buff.index);
                this->gdrom.pio_buff.data[this->gdrom.pio_buff.index] = (u16)val;
                this->gdrom.pio_buff.index += 1;
                if (this->gdrom.pio_buff.size == this->gdrom.pio_buff.index)
                {
                    assert(this->gdrom.pio_buff.next_state != gds_pio_get_data);
                    GDROM_setstate(this, this->gdrom.pio_buff.next_state);
                }
            }
            else
            {
                gd_printf("GDROM: Illegal Write to DATA\n");
            }
            return;
        case 0x005F7084: // Features
            gd_printf("\nGDROM SET FEATURES! %llu", this->sh4.trace_cycles);
            this->gdrom.features = val;
            gd_printf("\nGDROM SET FEATURES REG %02llx", val);
            return;
        case 0x005F7090: // Byte count lo
            this->gdrom.byte_count.lo = (u8)val;
            return;
        case 0x005F7094: // Byte count lo
            this->gdrom.byte_count.hi = (u8)val;
            return;
        case 0x005F7088: // Sector Count
            this->gdrom.sector_count.u = val;
            gd_printf("\nGDROM SET SECTOR COUNT transfer_mode %d   mode_value %d", this->gdrom.sector_count.transfer_mode, this->gdrom.sector_count.mode_value);
            return;
        case 0x005F709C: // ATA Command
        // OOPS
            this->gdrom.ata_cmd = val;
            GDROM_setstate(this, gds_procata);
            return;
    }
    *success = 0;
    printf("\nUnhandled GDR reg write %02x val %04llx", addr, val);
}

u64 GDROM_read(struct DC* this, u32 addr, u32 bits, u32* success)
{
    addr &= 0x1FFFFFFF;
    switch(addr) {
        case 0x005F7018: // AltStatus
            gd_printf("\n%02x AltSTATUS", this->gdrom.device_status.u);
            return this->gdrom.device_status.u | (1 << 4);
        case 0x005F709C: // Status
            gd_printf("\n%02x STATUS", this->gdrom.device_status.u);
            gdrom_clear_interrupt(this);
            return this->gdrom.device_status.u | (1 << 4);
        case 0x005F7080: // DATA
            if (this->gdrom.pio_buff.index == this->gdrom.pio_buff.size)
            {
                gd_printf("\n-------------------------nGDROM: Illegal Read From DATA (underflow)\n");
            }
            else
            {
                u32 rv = this->gdrom.pio_buff.data[this->gdrom.pio_buff.index];
                this->gdrom.pio_buff.index += 1;
                this->gdrom.byte_count.u -= 2;
                if (this->gdrom.pio_buff.index == this->gdrom.pio_buff.size)
                {
                    assert(this->gdrom.pio_buff.next_state != gds_pio_send_data);
                    //end of pio transfer !
                    GDROM_setstate(this, this->gdrom.pio_buff.next_state);
                }
                return rv;
            }
            return 0;
        case 0x005F708C: // Sector number
            return this->gdrom.sector_number.u;
        case 0x005F7090: // Byte count lo
            return this->gdrom.byte_count.lo;
        case 0x005F7094: // Byte count hi
            return this->gdrom.byte_count.hi;

    }
    printf("\nUNKNOWN GDROM READ %8x", addr);
    *success = 0;
    return 0;
}