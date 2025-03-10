//
// Created by . on 1/22/25.
//

#include "nds_spi.h"
#include "nds_bus.h"
#include "nds_irq.h"
#include "helpers/multisize_memaccess.c"

#define SPI this->spi
#define PWM this->spi.pwm
#define FMW this->spi.firmware
#define TSC this->spi.touchscr

struct NDS_tsc_cd {
    i32 adc_x1;
    i32 adc_y1;
    i32 adc_x2;
    i32 adc_y2;
    i32 screen_x1;
    i32 screen_y1;
    i32 screen_x2;
    i32 screen_y2;
};

static void apply_calib_data(struct NDS *this, struct NDS_tsc_cd *data)
{
    struct NDS_SPI_TOUCHSCREEN *t = &this->spi.touchscr;
    t->adc_x_top_left = data->adc_x1;
    t->adc_x_delta = data->adc_x2 - data->adc_x1;
    t->adc_y_top_left = data->adc_y1;
    t->adc_y_delta = data->adc_y2 - data->adc_y1;
    t->screen_x_top_left = data->screen_x1;
    t->screen_x_delta = data->screen_x2 - data->screen_x1;
    t->screen_y_top_left = data->screen_y1;
    t->screen_y_delta = data->screen_y2 - data->screen_y1;

    if (!t->screen_x_delta) t->screen_x_delta = 1;
    if (!t->screen_y_delta) t->screen_y_delta = 1;
}

static void read_and_apply_touchscreen_calibration(struct NDS *this)
{
    u32 userdata_offset = cR16(this->mem.firmware, 0x20) << 3;
    u32 offset = userdata_offset + 0x58;

    struct NDS_tsc_cd calibration_data;

    calibration_data.adc_x1 = cR16(this->mem.firmware, offset); offset += 2;
    calibration_data.adc_y1 = cR16(this->mem.firmware, offset); offset += 2;
    calibration_data.screen_x1 = cR8(this->mem.firmware, offset); offset += 1;
    calibration_data.screen_y1 = cR8(this->mem.firmware, offset); offset += 1;
    calibration_data.adc_x2 = cR16(this->mem.firmware, offset); offset += 2;
    calibration_data.adc_y2 = cR16(this->mem.firmware, offset); offset += 2;
    calibration_data.screen_x2 = cR8(this->mem.firmware, offset); offset += 1;
    calibration_data.screen_y2 = cR8(this->mem.firmware, offset); offset += 1;

    apply_calib_data(this, &calibration_data);
}

void NDS_SPI_reset(struct NDS *this)
{
    this->spi.enable = 1;
    read_and_apply_touchscreen_calibration(this);
}

// On read OR write, value is transferred in and data is transferred out

static void pwm_transaction(struct NDS *this, u32 val)
{
    if (!PWM.hold) {
        PWM.hold = 1;
        PWM.index = val & 0x7F;
        PWM.pos = 1;
        PWM.dir = (val >> 7) & 1; //0=write, 1=read
        return;
    }
    if (PWM.pos == 1) {
        u32 read = PWM.dir;
        switch (PWM.index) {
            case 0: // power management control
                if (read) SPI.output = PWM.regs[0];
                else
                    PWM.regs[0] = val & 0x7F; // bit 8 "always zero" check
                break;
            case 1: // battery status LED
                if (read) SPI.output = PWM.regs[1];
                else
                    PWM.regs[1] = val & 1;
                break;
            case 2: // microphone amp control
                if (read) SPI.output = PWM.regs[2];
                else
                    PWM.regs[2] = val & 1;
                break;
            case 3: //microphone amp gain control
                if (read) SPI.output = PWM.regs[3];
                else
                    PWM.regs[3] = val & 3;
                break;
            default:
                printf("\nUnknown PWM read:%d %d", read, PWM.index);
                SPI.output = 0;
                break;
        }
    }
    else
        SPI.output = 0;
}

static void firmware_transaction(struct NDS *this, u32 val)
{
    if (!FMW.hold) {
        FMW.hold = 1;
        FMW.cmd.cur = val;
        FMW.cmd.pos = 1;
        FMW.cmd.addr = 0;

        switch(FMW.cmd.cur) {
            case 4: // write disable...
                printf("\nFIRMWARE write disable!");
                FMW.status &= 2;
                SPI.output = 0;
                break;
            case 6: // write enable...
                printf("\nFIRMWARE write enable!");
                FMW.status |= 2;
                SPI.output = 0;
                break;
        }
        return;
    }

    switch(FMW.cmd.cur) {
        case 5: // read status register
            SPI.output = FMW.status;
            break;
        case 0x9F: // read ID
            switch(FMW.cmd.pos) {
                case 1:
                    SPI.output = 0x20;
                    break;
                case 2:
                    SPI.output = 0x40;
                    break;
                case 3:
                    SPI.output = 0x12;
                    break;
                default:
                    SPI.output = 0;
                    break;
            }
            FMW.cmd.pos++;
            break;
        case 3: // Read
            if (FMW.cmd.pos < 4) {
                FMW.cmd.addr = (FMW.cmd.addr << 8) | val;
                SPI.output = 0;
            }
            else {
                SPI.output = this->mem.firmware[FMW.cmd.addr & 0x3FFFF];
                FMW.cmd.addr++;
            }
            FMW.cmd.pos++;
            break;
        case 10: // write!?
            if (FMW.cmd.pos < 4) {
                FMW.cmd.addr = (FMW.cmd.addr << 8) | val;
                SPI.output = 0;
            }
            else {
                this->mem.firmware[FMW.cmd.addr] = val;
                SPI.output = val;
                FMW.cmd.addr++;
            }
            FMW.cmd.pos++;
            break;
        default:
            printf("\nUnknown firmware command %02x", FMW.cmd.cur);
            SPI.output = 0;
            break;
    }
}

static void touchscreen_transaction(struct NDS *this, u32 val)
{
    //SPI.output = (TSC.result >> (15 - TSC.pos)) & 1;
    //SPI.output = 0x20 >> 1; // 0x10's leads to 512, 32
    // 512 is 0x200
    // 32 is 0x20
    // So, I think, return >>5
    // Then return <<3?
    switch(TSC.pos) {
        case 1:
            SPI.output = (TSC.result >> 5) & 0xFF;
            break;
        case 2:
            SPI.output = (TSC.result << 3) & 0xFF;
            break;
        default:
            SPI.output = 0;
            break;
    }


    if (val & 0x80) { // new command byte
        struct JSM_TOUCHSCREEN *ts = &this->spi.touchscr.pio->touchscreen;
        TSC.cnt.u = val;
        TSC.cnt.penirq ^= 1;
        TSC.pos = 0;
        switch(TSC.cnt.chan_select) {
            case 1:
                //  1 Touchscreen Y-Position  (somewhat 0B0h..F20h, or FFFh=released)
                if (ts->touch.down) {
                    TSC.result = (u16)((ts->touch.y - TSC.screen_y_top_left + 1) * TSC.adc_y_delta / TSC.screen_y_delta + TSC.adc_y_top_left);
                }
                else
                    TSC.result = 0;
                break;
            case 5:
                //   5 Touchscreen X-Position  (somewhat 100h..ED0h, or 000h=released)
                if (ts->touch.down) {
                    TSC.result = (u16)((ts->touch.x - TSC.screen_x_top_left + 1) * TSC.adc_x_delta / TSC.screen_x_delta + TSC.adc_x_top_left);
                }
                else
                    TSC.result = 0x0FFF;
                break;

            case 6: // mic
                TSC.result = 0x800;
                break;

            default:
                TSC.result = 0xFFF;
        }
        if (TSC.cnt.conversion_mode) TSC.result &= 0xFF0; // 8-bit conversion mode
    }
    TSC.pos++;
}

void NDS_SPI_release_hold(struct NDS *this)
{
    switch(SPI.cnt.device) {
        case 0:
            PWM.hold = 0;
            return;
        case 1:
            FMW.hold = 0;
            FMW.cmd.cur = 0;
            return;
        case 2:
            TSC.hold = 0;
            return;
    }
}

static void SPI_irq(void *ptr, u64 num_cycles, u64 clock, u32 jitter)
{
    struct NDS *this = (struct NDS *)ptr;
    this->spi.irq_id = 0;
    if (SPI.cnt.irq_enable)
        NDS_update_IF7(this, NDS_IRQ_SPI);
}


static void SPI_transaction(struct NDS *this, u32 val)
{
    SPI.input = val;
    SPI.output = 0;
    if (!SPI.enable) {
        printf("\nSPI DISABLED!");
        return;
    }
    u32 old_chipsel = SPI.cnt.chipselect_hold;
    SPI.chipsel |= SPI.cnt.chipselect_hold;
    switch(SPI.cnt.device) {
        case 0:
            pwm_transaction(this, val);
            break;
        case 1:
            firmware_transaction(this, val);
            break;
        case 2:
            touchscreen_transaction(this, val);
            break;
        case 3:
            printf("\nINVALID SPI TRANSACTION!?");
            break;
    }

    SPI.chipsel &= SPI.cnt.chipselect_hold;
    if (!SPI.chipsel) { // Chipsel down
        NDS_SPI_release_hold(this);
    }
    SPI.busy_until = NDS_clock_current7(this) + (8 * (8 << SPI.cnt.baudrate));

    if (SPI.irq_id) scheduler_delete_if_exist(&this->scheduler, SPI.irq_id);
    // Schedule IRQ
    SPI.irq_id = scheduler_add_or_run_abs(&this->scheduler, SPI.busy_until, 0, this, &SPI_irq, NULL);
}

u32 NDS_SPI_read(struct NDS *this, u32 sz)
{
    //SPI_transaction(this, 0);
    return this->spi.output;
}

void NDS_SPI_write(struct NDS *this, u32 sz, u32 val)
{
    SPI_transaction(this, val & 0xFF);
}
