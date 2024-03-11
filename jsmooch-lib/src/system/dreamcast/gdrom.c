//
// Created by David Schneider on 3/2/24.
//

#include "assert.h"
#include "stdio.h"
#include "dreamcast.h"
#include "gdrom.h"
#include "holly.h"

// LOTS of help/code from Reicast

static void gdrom_clear_interrupt(struct DC* this)
{
    // TODO: this
}

enum gd_states {
    //Generic
    gds_waitcmd,
    gds_procata,
    gds_waitpacket,
    gds_procpacket,
    gds_pio_send_data,
    gds_pio_get_data,
    gds_pio_end,
    gds_procpacketdone,

    //Command spec.
    gds_readsector_pio,
    gds_readsector_dma,
    gds_process_set_mode,
};

enum GDstatus {
    GD_BUSY = 0,
    GD_PAUSE,
    GD_STANDBY,
    GD_SEEK,
    GD_SCAN,
    GD_OPEN,
    GD_NODISC,
    GD_RETRY,
    GD_ERROR
};

void GDROM_init(struct DC* this)
{
    this->gdrom.interrupt_reason.u = 0;
}

static void GDROM_setdisc(struct DC* this) {
    this->gdrom.cdda.playing = 0;
    this->gdrom.sns_asc = 0x28;
    this->gdrom.sns_ascq = 0;
    this->gdrom.sns_key = 6;
    if (this->gdrom.sector_number.status == GD_BUSY)
        this->gdrom.sector_number.status = GD_PAUSE;
    else
        this->gdrom.sector_number.status = GD_BUSY;
    this->gdrom.sector_number.disc_format = 8; // GDROM

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
        default:
            printf("\nUNIMPLEMENTED STATE %d", state);
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

static void GDROM_command(struct DC* this, u32 cmd, u32* success)
{
    switch(cmd) {
        /*case 0x00:
            printf("\nGDROM NOP!");
            this->gdrom.device_status.BSY = 0;
            // set ABORT and ERROR in status register
            GDROM_set_interrupt(this);
            return;*/
        case 0x08:
            printf("\nGDROM SOFT RESET!");
            GDROM_reset(this);
            return;
        case 0xA0: // Wait for SPI packet!!!
            printf("\nGDROM WAIT FOR PACKET!");
            GDROM_setstate(this, gds_waitpacket);
            return;
        case 0xEF:
            printf("\nGDROM SET FEATURES! %llu", this->sh4.trace_cycles);
            this->gdrom.device_status.DSC = 0;
            this->gdrom.device_status.CHECK = 0;
            this->gdrom.device_status.DF = 0;

            this->gdrom.error.ABORT = 0;
            holly_raise_interrupt(this, hirq_gdrom_cmd);
            GDROM_setstate(this, gds_waitcmd);
            return;
    }
    *success = 0;
    printf("\nUNKNOWN GDROM COMMAND %02x", cmd);
}

void GDROM_write(struct DC* this, u32 addr, u64 val, u32 sz, u32* success)
{
    addr &= 0x1FFFFFFF;
    //printf("\nGDROM write! %08x %02x %04x %d cycle:%llu", reg | 0x5F7400, reg, val, bits, this->sh4.trace_cycles);
    switch(addr) {
        // read / write
        case 0x005F7018: // Device control
            this->gdrom.device_control.u = val & 2;
            printf("\nGDROM SET INTERRUPT %llu", val & 2);
            return;
        case 0x005F7084: // Features
            this->gdrom.features = val;
            printf("\nGDROM SET FEATURES REG %02llx", val);
            return;
        case 0x005F7088: // Sector Count
            this->gdrom.sector_count.u = val;
            printf("\nGDROM SET SECTOR COUNT transfer_mode %d   mode_value %d", this->gdrom.sector_count.transfer_mode, this->gdrom.sector_count.mode_value);
            return;
        case 0x005F709C: // Command
            GDROM_command(this, val, success);
            return;
    }
    *success = 0;
    printf("\nUnhandled GDR reg write %02x val %04llx", addr, val);
}

u64 GDROM_read(struct DC* this, u32 addr, u32 bits, u32* success)
{
    addr &= 0x1FFFFFFF;
    switch(addr) {
        case 0x005F7018: // Status (read)
        case 0x005F709C:
            gdrom_clear_interrupt(this);
            printf("\n%02x STATUS", this->gdrom.device_status.u);
            return this->gdrom.device_status.u;
    }
    *success = 0;
    return 0;
}