//
// Created by . on 1/22/25.
//

#include "nds_spi.h"
#include "nds_bus.h"
#include "nds_irq.h"
#include "helpers/multisize_memaccess.cpp"

#define SPI spi
#define PWM spi.pwm
#define FMW spi.firmware
#define TSC spi.touchscr

namespace NDS {


void core::SPI_apply_calib_data(tsc_cd &data)
{
    auto &t = spi.touchscr;
    t.adc_x_top_left = data.adc_x1;
    t.adc_x_delta = data.adc_x2 - data.adc_x1;
    t.adc_y_top_left = data.adc_y1;
    t.adc_y_delta = data.adc_y2 - data.adc_y1;
    t.screen_x_top_left = data.screen_x1;
    t.screen_x_delta = data.screen_x2 - data.screen_x1;
    t.screen_y_top_left = data.screen_y1;
    t.screen_y_delta = data.screen_y2 - data.screen_y1;

    if (!t.screen_x_delta) t.screen_x_delta = 1;
    if (!t.screen_y_delta) t.screen_y_delta = 1;
}

void core::SPI_read_and_apply_touchscreen_calibration()
{
    u32 userdata_offset = cR16(mem.firmware, 0x20) << 3;
    u32 offset = userdata_offset + 0x58;

    tsc_cd calibration_data;

    calibration_data.adc_x1 = cR16(mem.firmware, offset); offset += 2;
    calibration_data.adc_y1 = cR16(mem.firmware, offset); offset += 2;
    calibration_data.screen_x1 = cR8(mem.firmware, offset); offset += 1;
    calibration_data.screen_y1 = cR8(mem.firmware, offset); offset += 1;
    calibration_data.adc_x2 = cR16(mem.firmware, offset); offset += 2;
    calibration_data.adc_y2 = cR16(mem.firmware, offset); offset += 2;
    calibration_data.screen_x2 = cR8(mem.firmware, offset); offset += 1;
    calibration_data.screen_y2 = cR8(mem.firmware, offset); offset += 1;

    SPI_apply_calib_data(calibration_data);
}

void core::SPI_reset()
{
    spi.enable = 1;
    SPI_read_and_apply_touchscreen_calibration();
}

// On read OR write, value is transferred in and data is transferred out

void core::SPI_pwm_transaction(u32 val)
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

void core::SPI_firmware_transaction(u32 val)
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
                SPI.output = mem.firmware[FMW.cmd.addr & 0x3FFFF];
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
                mem.firmware[FMW.cmd.addr] = val;
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

void core::SPI_touchscreen_transaction(u32 val)
{
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
        JSM_TOUCHSCREEN *ts = &spi.touchscr.pio->touchscreen;
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

void core::SPI_release_hold()
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

void SPI_irq(void *ptr, u64 num_cycles, u64 clock, u32 jitter)
{
    auto *th = static_cast<core *>(ptr);
    th->spi.irq_id = 0;
    if (th->SPI.cnt.irq_enable)
        th->update_IF7(IRQ_SPI);
}


void core::SPI_transaction(u32 val)
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
            SPI_pwm_transaction(val);
            break;
        case 1:
            SPI_firmware_transaction(val);
            break;
        case 2:
            SPI_touchscreen_transaction(val);
            break;
        case 3:
            printf("\nINVALID SPI TRANSACTION!?");
            break;
    }

    SPI.chipsel &= SPI.cnt.chipselect_hold;
    if (!SPI.chipsel) { // Chipsel down
        SPI_release_hold();
    }
    SPI.busy_until = clock.current7() + (8 * (8 << SPI.cnt.baudrate));

    if (SPI.irq_id) scheduler.delete_if_exist(SPI.irq_id);
    // Schedule IRQ
    SPI.irq_id = scheduler.add_or_run_abs(SPI.busy_until, 0, this, &SPI_irq, nullptr);
}

u32 core::SPI_read(u8 sz)
{
    //SPI_transaction, 0);
    return spi.output;
}

void core::SPI_write(u8 sz, u32 val)
{
    SPI_transaction(val & 0xFF);
}
}