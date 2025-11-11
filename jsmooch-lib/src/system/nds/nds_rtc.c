//
// Created by . on 1/21/25.
//

#include <string.h>

#include "nds_rtc.h"
#include "nds_bus.h"
#include "nds_irq.h"
#include "helpers/scheduler.h"

/*
 * this code pretty closely follows MelonDS.
 * I just could not be bothere to care about doing this, sorry.
 */

#define RTC this->io.rtc

static u32 bcd_inc(u32 v) {
    v++;
    if ((v & 0x0F) >= 10) {
        v = (v & 0xF0) + 0x10;
    }
    return v;
}

static u32 days_in_month(NDS *this)
{
    u8 numdays;

    switch (this->io.rtc.date_time[1])
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
            int year = this->io.rtc.date_time[0];
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

static void set_irq(NDS *this, u32 which)
{
    u8 oldstat = this->io.rtc.irq_flag;
    this->io.rtc.irq_flag |= which;
    this->io.rtc.status_reg[0] |= which;

    if ((!(oldstat & 0x30) && (this->io.rtc.irq_flag & 0x30))) {
        if ((this->io.sio.rcnt & 0xC100) == 0x8100) {
            NDS_update_IF7(this, NDS_IRQ_SERIAL);
        }
    }
}

static void clear_irq(NDS *this, u32 flag)
{
    this->io.rtc.irq_flag &= ~flag;
}

static void process_irqs(NDS *this, u32 kind) {
    // process int1
    switch (this->io.rtc.status_reg[1] & 0x0F) {
        case 0: // none
            if (kind == 2) {
                clear_irq(this, 0x10);
            }
            break;

        case 1:
        case 5: // selected frequency steady interrupt
            if ((kind == 1 && ((this->io.rtc.divider & 0x3FF) == 0)) || (kind == 2)) {
                u32 mask = 0;
                if (this->io.rtc.alarm1[2] & 1) mask |= 0x4000;
                if (this->io.rtc.alarm1[2] & 2) mask |= 0x2000;
                if (this->io.rtc.alarm1[2] & 4) mask |= 0x1000;
                if (this->io.rtc.alarm1[2] & 8) mask |= 0x0800;
                if (this->io.rtc.alarm1[2] & 0x10) mask |= 0x0400;

                if (mask && ((this->io.rtc.divider & mask) != mask))
                    set_irq(this, 0x10);
                else
                    clear_irq(this, 0x10);
            }
            break;

        case 2:
        case 6: // per-minute edge interrupt
            if ((kind == 0) || (kind == 2 && (this->io.rtc.irq_flag & 1))) {
                set_irq(this, 0x10);
            }
            break;

        case 3: // per-minute steady interrupt 1 (duty 30s)
            if ((kind == 0) || (kind == 2 && (this->io.rtc.irq_flag & 1))) {
                set_irq(this, 0x10);
            } else if ((kind == 1) && (this->io.rtc.date_time[6] == 0x30) && ((this->io.rtc.divider & 0x7FFF) == 0)) {
                clear_irq(this, 0x10);
            }
            break;

        case 7: // per-minute steady interrupt 2 (duty 256 cycles)
            if ((kind == 0) || (kind == 2 && (this->io.rtc.irq_flag & 1))) {
                set_irq(this, 0x10);
            } else if ((kind == 1) && (this->io.rtc.date_time[6] == 0x00) && ((this->io.rtc.divider & 0x7FFF) == 256)) {
                clear_irq(this, 0x10);
            }
            break;

        case 4: // alarm interrupt
            if (kind == 0) {
                u32 cond = 1;
                if (this->io.rtc.alarm1[0] & 0x80)
                    cond = cond && ((this->io.rtc.alarm1[0] & 0x07) == this->io.rtc.date_time[3]);
                if (this->io.rtc.alarm1[1] & 0x80)
                    cond = cond && ((this->io.rtc.alarm1[1] & 0x7F) == this->io.rtc.date_time[4]);
                if (this->io.rtc.alarm1[2] & 0x80)
                    cond = cond && ((this->io.rtc.alarm1[2] & 0x7F) == this->io.rtc.date_time[5]);

                if (cond)
                    set_irq(this, 0x10);
                else
                    clear_irq(this, 0x10);
            }
            break;

        default: // 32KHz output
            if (kind == 1) {
                set_irq(this, 0x10);
                clear_irq(this, 0x10);
            }
            break;
    }

    // INT2

    if (this->io.rtc.status_reg[1] & 0x40) {
        // alarm interrupt
        if (kind == 0) {
            bool cond = true;
            if (this->io.rtc.alarm2[0] & 0x80)
                cond = cond && ((this->io.rtc.alarm2[0] & 0x07) == this->io.rtc.date_time[3]);
            if (this->io.rtc.alarm2[1] & 0x80)
                cond = cond && ((this->io.rtc.alarm2[1] & 0x7F) == this->io.rtc.date_time[4]);
            if (this->io.rtc.alarm2[2] & 0x80)
                cond = cond && ((this->io.rtc.alarm2[2] & 0x7F) == this->io.rtc.date_time[5]);

            if (cond)
                set_irq(this, 0x20);
            else
                clear_irq(this, 0x20);
        }
    } else {
        if (kind == 2) {
            clear_irq(this, 0x20);
        }
    }
}

void NDS_RTC_reset(NDS *this)
{
    this->io.rtc.input = 0;
    this->io.rtc.input_bit = this->io.rtc.input_pos = 0;

    memset(this->io.rtc.output, 0, sizeof(this->io.rtc.output));
    this->io.rtc.output_pos = this->io.rtc.output_bit = 0;

    this->io.rtc.cmd = 0;

    this->io.rtc.divider = 0;
    scheduler_add_or_run_abs(&this->scheduler, 32768, 0, this, &NDS_RTC_tick, NULL);
}

void NDS_RTC_init(NDS *this)
{
    this->io.rtc.status_reg[0] = 0x82;
}

static void check_end_of_month(NDS *this)
{
    if (this->io.rtc.date_time[2] > days_in_month(this)) {
        this->io.rtc.date_time[2] = 1;
        this->io.rtc.date_time[1] = bcd_inc(this->io.rtc.date_time[1]);
        if (this->io.rtc.date_time[1] > 0x12)
        {
            this->io.rtc.date_time[1] = 1;
            this->io.rtc.date_time[0] = bcd_inc(this->io.rtc.date_time[0]);
        }
    }
}

static void cmd_read(NDS *this)
{
    if ((this->io.rtc.cmd & 0x0F) == 6)
    {
        switch (this->io.rtc.cmd & 0x70)
        {
            case 0: // read status reg 1. clear some bits
                this->io.rtc.output[0] = this->io.rtc.status_reg[0];
                this->io.rtc.status_reg[0] &= 15;
                break;

            case 0x40:
                this->io.rtc.output[0] = this->io.rtc.status_reg[1];
                break;

            case 0x20:
                memcpy(this->io.rtc.output, &this->io.rtc.date_time[0], 7);
                break;

            case 0x60:
                memcpy(this->io.rtc.output, &this->io.rtc.date_time[4], 3);
                break;

            case 0x10:
                if (this->io.rtc.status_reg[1] & 4)
                    memcpy(this->io.rtc.output, &this->io.rtc.alarm1[0], 3);
                else
                    this->io.rtc.output[0] = this->io.rtc.alarm1[2];
                break;

            case 0x50:
                memcpy(this->io.rtc.output, &this->io.rtc.alarm2[0], 3);
                break;

            case 0x30: this->io.rtc.output[0] = this->io.rtc.clock_adjust; break;
            case 0x70: this->io.rtc.output[0] = this->io.rtc.free_register; break;
        }

        return;
    }
}

static void reset_state(NDS *this)
{
    this->io.rtc.date_time[0] = 0;
    this->io.rtc.date_time[1] = 1;
    this->io.rtc.date_time[2] = 1;
    this->io.rtc.date_time[3] = 0;
    this->io.rtc.date_time[4] = 0;
    this->io.rtc.date_time[5] = 0;
    this->io.rtc.date_time[6] = 0;
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

static void write_dt(NDS *this, u32 num, u32 val) {
    switch (num) {
        case 1: // year
            RTC.date_time[0] = bcd_correct(val, 0x00, 0x99);
            break;

        case 2: // month
            RTC.date_time[1] = bcd_correct(val & 0x1F, 0x01, 0x12);
            break;

        case 3: // day
            RTC.date_time[2] = bcd_correct(val & 0x3F, 0x01, 0x31);
            check_end_of_month(this);
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

static void save_dt(NDS *this)
{
    printf("\nRTC DateTime save not support yet!");
}


static void cmd_write(NDS *this, u32 val) {
    if ((this->io.rtc.cmd & 0x0F) == 6) {
        switch (this->io.rtc.cmd & 0x70) {
            case 0x00:
                if (this->io.rtc.input_pos == 1) {
                    u8 oldval = this->io.rtc.status_reg[0];

                    if (val & 1) {// reset
                        reset_state(this);
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
                      process_irqs(this, 2);
                }
                break;

            case 0x20:
                if (RTC.input_pos <= 7)
                    write_dt(this, RTC.input_pos, val);
                if (RTC.input_pos == 7)
                    save_dt(this);
                break;

            case 0x60:
                if (RTC.input_pos <= 3)
                    write_dt(this, RTC.input_pos + 4, val);
                if (RTC.input_pos == 3)
                    save_dt(this);
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

static void RTC_byte_in(NDS *this)
{
    u8 val = this->io.rtc.input;
    if (this->io.rtc.input_pos == 0) { // First read byte. Command?
        if ((val & 0xF0) == 0x60) // command is in reverse
        {
            u8 rev[16] = {0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6};
            this->io.rtc.cmd = rev[val & 0xF];
        }
        else
            this->io.rtc.cmd = val;
        if (this->io.rtc.cmd & 0x80) cmd_read(this);
        return;
    }
    cmd_write(this, val);
}

void NDS_write_RTC(NDS *this, u32 sz, u32 val)
{
    if (sz != 1) val |= (this->io.rtc.data & 0xFF00);

    if (val & 4) {
        if ((this->io.rtc.data & 0b0100) == 0) {
            this->io.rtc.input = 0;
            this->io.rtc.input_bit = 0;
            this->io.rtc.input_pos = 0;

            memset(this->io.rtc.output, 0, sizeof(this->io.rtc.output));
            this->io.rtc.output_bit = 0;
            this->io.rtc.output_pos = 0;
        }
        else {
            if ((val & 2) == 0) { // low clock...
                if (val & 16) {
                    // write
                    if (val & 1)
                        this->io.rtc.input |= 1 << this->io.rtc.input_bit++;

                    if (this->io.rtc.input_bit >= 8) {
                        this->io.rtc.input_bit = 0;
                        RTC_byte_in(this);
                        this->io.rtc.input = 0;
                        this->io.rtc.input_pos++;
                    }
                }
                else
                {
                    // read
                    if (this->io.rtc.output[this->io.rtc.output_pos] & ( 1 << this->io.rtc.output_bit))
                        this->io.rtc.data |= 0x0001;
                    else
                        this->io.rtc.data &= 0xFFFE;

                    this->io.rtc.output_bit++;
                    if (this->io.rtc.output_bit >= 8)
                    {
                        this->io.rtc.output_bit = 0;
                        if (this->io.rtc.output_pos < 7)
                            this->io.rtc.output_pos++;
                    }
                }
            }
        }
    }

    if (val & 16)
        this->io.rtc.data = val;
    else
        this->io.rtc.data = (this->io.rtc.data & 1) | (val & 0xFFFE);

}

static void day_inc(NDS *this) {
    // day-of-week counter
    this->io.rtc.date_time[3]++;
    if (this->io.rtc.date_time[3] >= 7)
        this->io.rtc.date_time[3] = 0;

    // day counter
    this->io.rtc.date_time[2] = bcd_inc(this->io.rtc.date_time[2]);
    check_end_of_month(this);
}

#define MASTER_CYCLES_PER_FRAME 570716

void NDS_RTC_tick(void *ptr, u64 key, u64 clock, u32 jitter) // Called on scanline start
{
    struct NDS *this = (NDS *)ptr;
    u64 tstamp = (NDS_clock_current7(this) - jitter) + 32768;
    this->io.rtc.sch_id = scheduler_add_or_run_abs(&this->scheduler, tstamp, 0, this, &NDS_RTC_tick, NULL);
    this->io.rtc.divider++;
    if ((this->io.rtc.divider & 0x7FFF) == 0) {
        this->io.rtc.date_time[6] = bcd_inc(this->io.rtc.date_time[6]);
        if (this->io.rtc.date_time[6] >= 0x60) { // 60 seconds...
            this->io.rtc.date_time[6] = 0;
            this->io.rtc.minute_count++;
            this->io.rtc.date_time[5] = bcd_inc(this->io.rtc.date_time[5]);
            if (this->io.rtc.date_time[5] >= 0x60) { // 60 minutes...
                this->io.rtc.date_time[5] = 0;
                u8 hour = bcd_inc(this->io.rtc.date_time[4] & 0x3F);
                u8 pm = this->io.rtc.date_time[4] & 0x40;

                if (this->io.rtc.status_reg[0] & 2) {
                    // 24-hour mode

                    if (hour >= 0x24) {
                        hour = 0;
                        day_inc(this);
                    }

                    pm = (hour >= 0x12) ? 0x40 : 0;
                }
                else {
                    // 12-hour mode

                    if (hour >= 0x12)
                    {
                        hour = 0;
                        if (pm) day_inc(this);
                        pm ^= 0x40;
                    }
                }

                this->io.rtc.date_time[4] = hour | pm;
            }

            this->io.rtc.irq_flag |= 1;
            process_irqs(this, 0);
        }
    }

    if ((this->io.rtc.divider & 0x7FFF) == 4) {
        RTC.irq_flag &= ~1;
    }

    process_irqs(this, 1);
}
