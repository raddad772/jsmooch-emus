//
// Created by . on 1/21/25.
//

#include "nds_rtc.h"
#include "nds_bus.h"
#include "nds_irq.h"
#include "helpers/scheduler.h"
namespace NDS {
/*
 * this code pretty closely follows MelonDS.
 * I just could not be bothere to care about doing this, sorry.
 */

#define RTC io.rtc

static u32 bcd_inc(u32 v) {
    v++;
    if ((v & 0x0F) >= 10) {
        v = (v & 0xF0) + 0x10;
    }
    return v;
}

u32 core::RTC_days_in_month() const {
    u8 numdays;

    switch (io.rtc.date_time[1])
    {
        case 0x01: // Jan
        case 0x03: // Mar
        case 0x05: // May
        case 0x07: // Jul
        case 0x08: // Aug
        case 0x10: // Oct
        case 0x12: // Dec
            numdays = 0x31;
            break;

        case 0x04: // Apr
        case 0x06: // Jun
        case 0x09: // Sep
        case 0x11: // Nov
            numdays = 0x30;
            break;

        case 0x02: // Feb
        {
            numdays = 0x28;

            // leap year: if year divisible by 4 and not divisible by 100 unless divisible by 400
            // the limited year range (2000-2099) simplifies this
            int year = io.rtc.date_time[0];
            year = (year & 0xF) + ((year >> 4) * 10);
            if (!(year & 3))
                numdays = 0x29;
        }
            break;

        default:
            return 0;
    }

    return numdays;
}

void core::RTC_set_irq(u32 which)
{
    u8 oldstat = io.rtc.irq_flag;
    io.rtc.irq_flag |= which;
    io.rtc.status_reg[0] |= which;

    if ((!(oldstat & 0x30) && (io.rtc.irq_flag & 0x30))) {
        if ((io.sio.rcnt & 0xC100) == 0x8100) {
            update_IF7(IRQ_SERIAL);
        }
    }
}

void core::RTC_clear_irq(u32 flag)
{
    io.rtc.irq_flag &= ~flag;
}

void core::RTC_process_irqs(u32 kind) {
    // process int1
    switch (io.rtc.status_reg[1] & 0x0F) {
        case 0: // none
            if (kind == 2) {
                RTC_clear_irq(0x10);
            }
            break;

        case 1:
        case 5: // selected frequency steady interrupt
            if ((kind == 1 && ((io.rtc.divider & 0x3FF) == 0)) || (kind == 2)) {
                u32 mask = 0;
                if (io.rtc.alarm1[2] & 1) mask |= 0x4000;
                if (io.rtc.alarm1[2] & 2) mask |= 0x2000;
                if (io.rtc.alarm1[2] & 4) mask |= 0x1000;
                if (io.rtc.alarm1[2] & 8) mask |= 0x0800;
                if (io.rtc.alarm1[2] & 0x10) mask |= 0x0400;

                if (mask && ((io.rtc.divider & mask) != mask))
                    RTC_set_irq(0x10);
                else
                    RTC_clear_irq(0x10);
            }
            break;

        case 2:
        case 6: // per-minute edge interrupt
            if ((kind == 0) || (kind == 2 && (io.rtc.irq_flag & 1))) {
                RTC_set_irq(0x10);
            }
            break;

        case 3: // per-minute steady interrupt 1 (duty 30s)
            if ((kind == 0) || (kind == 2 && (io.rtc.irq_flag & 1))) {
                RTC_set_irq(0x10);
            } else if ((kind == 1) && (io.rtc.date_time[6] == 0x30) && ((io.rtc.divider & 0x7FFF) == 0)) {
                RTC_clear_irq(0x10);
            }
            break;

        case 7: // per-minute steady interrupt 2 (duty 256 cycles)
            if ((kind == 0) || (kind == 2 && (io.rtc.irq_flag & 1))) {
                RTC_set_irq(0x10);
            } else if ((kind == 1) && (io.rtc.date_time[6] == 0x00) && ((io.rtc.divider & 0x7FFF) == 256)) {
                RTC_clear_irq(0x10);
            }
            break;

        case 4: // alarm interrupt
            if (kind == 0) {
                u32 cond = 1;
                if (io.rtc.alarm1[0] & 0x80)
                    cond = cond && ((io.rtc.alarm1[0] & 0x07) == io.rtc.date_time[3]);
                if (io.rtc.alarm1[1] & 0x80)
                    cond = cond && ((io.rtc.alarm1[1] & 0x7F) == io.rtc.date_time[4]);
                if (io.rtc.alarm1[2] & 0x80)
                    cond = cond && ((io.rtc.alarm1[2] & 0x7F) == io.rtc.date_time[5]);

                if (cond)
                    RTC_set_irq(0x10);
                else
                    RTC_clear_irq(0x10);
            }
            break;

        default: // 32KHz output
            if (kind == 1) {
                RTC_set_irq(0x10);
                RTC_clear_irq(0x10);
            }
            break;
    }

    // INT2

    if (io.rtc.status_reg[1] & 0x40) {
        // alarm interrupt
        if (kind == 0) {
            bool cond = true;
            if (io.rtc.alarm2[0] & 0x80)
                cond = cond && ((io.rtc.alarm2[0] & 0x07) == io.rtc.date_time[3]);
            if (io.rtc.alarm2[1] & 0x80)
                cond = cond && ((io.rtc.alarm2[1] & 0x7F) == io.rtc.date_time[4]);
            if (io.rtc.alarm2[2] & 0x80)
                cond = cond && ((io.rtc.alarm2[2] & 0x7F) == io.rtc.date_time[5]);

            if (cond)
                RTC_set_irq(0x20);
            else
                RTC_clear_irq(0x20);
        }
    } else {
        if (kind == 2) {
            RTC_clear_irq(0x20);
        }
    }
}

void core::RTC_reset()
{
    io.rtc.input = 0;
    io.rtc.input_bit = io.rtc.input_pos = 0;

    memset(io.rtc.output, 0, sizeof(io.rtc.output));
    io.rtc.output_pos = io.rtc.output_bit = 0;

    io.rtc.cmd = 0;

    io.rtc.divider = 0;
    scheduler.add_or_run_abs(32768, 0, this, &RTC_tick, nullptr);
}

void core::RTC_check_end_of_month()
{
    if (io.rtc.date_time[2] > RTC_days_in_month()) {
        io.rtc.date_time[2] = 1;
        io.rtc.date_time[1] = bcd_inc(io.rtc.date_time[1]);
        if (io.rtc.date_time[1] > 0x12)
        {
            io.rtc.date_time[1] = 1;
            io.rtc.date_time[0] = bcd_inc(io.rtc.date_time[0]);
        }
    }
}

void core::RTC_cmd_read()
{
    if ((io.rtc.cmd & 0x0F) == 6)
    {
        switch (io.rtc.cmd & 0x70)
        {
            case 0: // read status reg 1. clear some bits
                io.rtc.output[0] = io.rtc.status_reg[0];
                io.rtc.status_reg[0] &= 15;
                break;

            case 0x40:
                io.rtc.output[0] = io.rtc.status_reg[1];
                break;

            case 0x20:
                memcpy(io.rtc.output, &io.rtc.date_time[0], 7);
                break;

            case 0x60:
                memcpy(io.rtc.output, &io.rtc.date_time[4], 3);
                break;

            case 0x10:
                if (io.rtc.status_reg[1] & 4)
                    memcpy(io.rtc.output, &io.rtc.alarm1[0], 3);
                else
                    io.rtc.output[0] = io.rtc.alarm1[2];
                break;

            case 0x50:
                memcpy(io.rtc.output, &io.rtc.alarm2[0], 3);
                break;

            case 0x30: io.rtc.output[0] = io.rtc.clock_adjust; break;
            case 0x70: io.rtc.output[0] = io.rtc.free_register; break;
        }

        return;
    }
}

void core::RTC_reset_state()
{
    io.rtc.date_time[0] = 0;
    io.rtc.date_time[1] = 1;
    io.rtc.date_time[2] = 1;
    io.rtc.date_time[3] = 0;
    io.rtc.date_time[4] = 0;
    io.rtc.date_time[5] = 0;
    io.rtc.date_time[6] = 0;
    for (u32 i = 0; i < 3; i++) {
        if (i < 2) RTC.status_reg[i] = 0;
        RTC.alarm_date1[i] = RTC.alarm_date2[i] = 0;
        RTC.alarm1[i] = RTC.alarm2[i] = 0;
    }
    RTC.clock_adjust = 0;
    RTC.free_register = 0;
    RTC.minute_count = 0;
    RTC.FOUT1 = RTC.FOUT2 = 0;
}

static u32 bcd_correct(u32 val, u32 vmin, u32 vmax)
{
    if (val < vmin || val > vmax)
        val = vmin;
    else if ((val & 0x0F) >= 0x0A)
        val = vmin;
    else if ((val & 0xF0) >= 0xA0)
        val = vmin;

    return val;    
}

void core::RTC_write_dt(u32 num, u32 val) {
    switch (num) {
        case 1: // year
            RTC.date_time[0] = bcd_correct(val, 0x00, 0x99);
            break;

        case 2: // month
            RTC.date_time[1] = bcd_correct(val & 0x1F, 0x01, 0x12);
            break;

        case 3: // day
            RTC.date_time[2] = bcd_correct(val & 0x3F, 0x01, 0x31);
            RTC_check_end_of_month();
            break;

        case 4: // day of week
            RTC.date_time[3] = bcd_correct(val & 0x07, 0x00, 0x06);
            break;

        case 5: // hour
        {
            u8 hour = val & 0x3F;
            u8 pm = val & 0x40;

            if (RTC.status_reg[0] & (1 << 1)) {
                // 24-hour mode

                hour = bcd_correct(hour, 0x00, 0x23);
                pm = (hour >= 0x12) ? 0x40 : 0;
            } else {
                // 12-hour mode

                hour = bcd_correct(hour, 0x00, 0x11);
            }

            RTC.date_time[4] = hour | pm;
        }
            break;

        case 6: // minute
            RTC.date_time[5] = bcd_correct(val & 0x7F, 0x00, 0x59);
            break;

        case 7: // second
            RTC.date_time[6] = bcd_correct(val & 0x7F, 0x00, 0x59);
            break;
    }
}

void core::RTC_save_dt()
{
    printf("\nRTC DateTime save not support yet!");
}


void core::RTC_cmd_write(u32 val) {
    if ((io.rtc.cmd & 0x0F) == 6) {
        switch (io.rtc.cmd & 0x70) {
            case 0x00:
                if (io.rtc.input_pos == 1) {
                    u8 oldval = io.rtc.status_reg[0];

                    if (val & 1) {// reset
                        RTC_reset_state();
                    }

                    RTC.status_reg[0] = (RTC.status_reg[0] & 0xF0) | (val & 0x0E);

                    if ((RTC.status_reg[0] ^ oldval) & 2) {
                        // AM/PM changed

                        u32 hour = RTC.date_time[4] & 0x3F;
                        u32 pm = RTC.date_time[4] & 0x40;

                        if (RTC.status_reg[0] & 2) {
                            // 24-hour mode

                            if (pm) {
                                hour += 0x12;
                                if ((hour & 0x0F) >= 0x0A)
                                    hour += 0x06;
                            }

                            hour = bcd_correct(hour, 0x00, 0x23);
                        } else {
                            // 12-hour mode

                            if (hour >= 0x12) {
                                pm = 0x40;

                                hour -= 0x12;
                                if ((hour & 0x0F) >= 0x0A)
                                    hour -= 0x06;
                            } else
                                pm = 0;

                            hour = bcd_correct(hour, 0x00, 0x11);
                        }

                        RTC.date_time[4] = hour | pm;
                    }
                }
                break;

            case 0x40:
                if (RTC.input_pos == 1) {
                    RTC.status_reg[1] = val;
                      RTC_process_irqs(2);
                }
                break;

            case 0x20:
                if (RTC.input_pos <= 7)
                    RTC_write_dt(RTC.input_pos, val);
                if (RTC.input_pos == 7)
                    RTC_save_dt();
                break;

            case 0x60:
                if (RTC.input_pos <= 3)
                    RTC_write_dt(RTC.input_pos + 4, val);
                if (RTC.input_pos == 3)
                    RTC_save_dt();
                break;

            case 0x10:
                if (RTC.status_reg[1] & 0x04) {
                    if (RTC.input_pos <= 3)
                        RTC.alarm1[RTC.input_pos - 1] = val;
                } else {
                    if (RTC.input_pos == 1)
                        RTC.alarm1[2] = val;
                }
                break;

            case 0x50:
                if (RTC.input_pos <= 3)
                    RTC.alarm2[RTC.input_pos - 1] = val;
                break;

            case 0x30:
                if (RTC.input_pos == 1)
                    RTC.clock_adjust = val;
                break;

            case 0x70:
                if (RTC.input_pos == 1)
                    RTC.free_register = val;
                break;
        }

        return;
    }
}

void core::RTC_byte_in()
{
    u8 val = io.rtc.input;
    if (io.rtc.input_pos == 0) { // First read byte. Command?
        if ((val & 0xF0) == 0x60) // command is in reverse
        {
            constexpr u8 rev[16] = {0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6};
            io.rtc.cmd = rev[val & 0xF];
        }
        else
            io.rtc.cmd = val;
        if (io.rtc.cmd & 0x80) RTC_cmd_read();
        return;
    }
    RTC_cmd_write(val);
}

void core::write_RTC(u8 sz, u32 val)
{
    if (sz != 1) val |= (io.rtc.data & 0xFF00);

    if (val & 4) {
        if ((io.rtc.data & 0b0100) == 0) {
            io.rtc.input = 0;
            io.rtc.input_bit = 0;
            io.rtc.input_pos = 0;

            memset(io.rtc.output, 0, sizeof(io.rtc.output));
            io.rtc.output_bit = 0;
            io.rtc.output_pos = 0;
        }
        else {
            if ((val & 2) == 0) { // low clock...
                if (val & 16) {
                    // write
                    if (val & 1)
                        io.rtc.input |= 1 << io.rtc.input_bit++;

                    if (io.rtc.input_bit >= 8) {
                        io.rtc.input_bit = 0;
                        RTC_byte_in();
                        io.rtc.input = 0;
                        io.rtc.input_pos++;
                    }
                }
                else
                {
                    // read
                    if (io.rtc.output[io.rtc.output_pos] & ( 1 << io.rtc.output_bit))
                        io.rtc.data |= 0x0001;
                    else
                        io.rtc.data &= 0xFFFE;

                    io.rtc.output_bit++;
                    if (io.rtc.output_bit >= 8)
                    {
                        io.rtc.output_bit = 0;
                        if (io.rtc.output_pos < 7)
                            io.rtc.output_pos++;
                    }
                }
            }
        }
    }

    if (val & 16)
        io.rtc.data = val;
    else
        io.rtc.data = (io.rtc.data & 1) | (val & 0xFFFE);

}

void core::RTC_day_inc() {
    // day-of-week counter
    io.rtc.date_time[3]++;
    if (io.rtc.date_time[3] >= 7)
        io.rtc.date_time[3] = 0;

    // day counter
    io.rtc.date_time[2] = bcd_inc(io.rtc.date_time[2]);
    RTC_check_end_of_month();
}

#define MASTER_CYCLES_PER_FRAME 570716

void core::RTC_tick(void *ptr, u64 key, u64 clock, u32 jitter) // Called on scanline start
{
    auto *bus = static_cast<core *>(ptr);
    u64 tstamp = (bus->clock.current7() - jitter) + 32768;
    bus->io.rtc.sch_id = bus->scheduler.add_or_run_abs(tstamp, 0, bus, &RTC_tick, nullptr);
    bus->io.rtc.divider++;
    if ((bus->io.rtc.divider & 0x7FFF) == 0) {
        bus->io.rtc.date_time[6] = bcd_inc(bus->io.rtc.date_time[6]);
        if (bus->io.rtc.date_time[6] >= 0x60) { // 60 seconds...
            bus->io.rtc.date_time[6] = 0;
            bus->io.rtc.minute_count++;
            bus->io.rtc.date_time[5] = bcd_inc(bus->io.rtc.date_time[5]);
            if (bus->io.rtc.date_time[5] >= 0x60) { // 60 minutes...
                bus->io.rtc.date_time[5] = 0;
                u8 hour = bcd_inc(bus->io.rtc.date_time[4] & 0x3F);
                u8 pm = bus->io.rtc.date_time[4] & 0x40;

                if (bus->io.rtc.status_reg[0] & 2) {
                    // 24-hour mode

                    if (hour >= 0x24) {
                        hour = 0;
                        bus->RTC_day_inc();
                    }

                    pm = (hour >= 0x12) ? 0x40 : 0;
                }
                else {
                    // 12-hour mode

                    if (hour >= 0x12)
                    {
                        hour = 0;
                        if (pm) bus->RTC_day_inc();
                        pm ^= 0x40;
                    }
                }

                bus->io.rtc.date_time[4] = hour | pm;
            }

            bus->io.rtc.irq_flag |= 1;
            bus->RTC_process_irqs(0);
        }
    }

    if ((bus->io.rtc.divider & 0x7FFF) == 4) {
        bus->RTC.irq_flag &= ~1;
    }

    bus->RTC_process_irqs(1);
}
}