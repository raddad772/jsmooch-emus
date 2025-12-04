
#include "mac_rtc.h"
#include "mac_bus.h"

namespace mac {
u8 RTC::read_bits()
{
    u8 o = 0;
    o |= (tx.shift >> 7) & 1;
    o |= old_clock_bit << 1;
    o |= (tx.kind > 0) << 2;
    return o;
}


void RTC::do_read()
{
    u8 rand = cmd & 0b01110000;
    u8 addr;
    switch(rand) {
        case 0b00100000:
            addr = ((cmd >> 2) & 3) + 0x10;
            tx.shift = param_mem[addr];
            break;
        case 0b01000000:
            addr = (cmd >> 2) & 0x0F;
            tx.shift = param_mem[addr];
            break;
        default: {
            switch (cmd) {
                case 0b10000001:
                    tx.shift = seconds & 0xFF;
                    break;
                case 0b10000101:
                    tx.shift = (seconds >> 8) & 0xFF;
                    break;
                case 0b10001001:
                    tx.shift = (seconds >> 16) & 0xFF;
                    break;
                case 0b10001101:
                    tx.shift = (seconds >> 24) & 0xFF;
                    break;
                default:
                    printf("\nUnknown RTC command %02x", cmd);
                    break;
            }
        }
    }
}

void RTC::finish_write()
{
    u8 rand = cmd & 0b01110000;
    u8 addr;
    switch(rand) {
        case 0b00100000:
            addr = ((cmd >> 2) & 3) + 0x10;
            tx.shift = param_mem[addr];
            break;
        case 0b01000000:
            addr = (cmd >> 2) & 0x0F;
            tx.shift = param_mem[addr];
            break;
        default: {
            switch (cmd) {
                case 0b10000001:
                    seconds = (seconds & 0xFFFFFF00) | tx.shift;
                    break;
                case 0b10000101:
                    seconds = (seconds & 0xFFFF00FF) | (tx.shift << 8);
                    break;
                case 0b10001001:
                    seconds = (seconds & 0xFF00FFFF) | (tx.shift << 16);
                    break;
                case 0b10001101:
                    seconds = (seconds & 0xFFFFFF) | (tx.shift << 24);
                    break;
                case 0b00110001:
                    test_register = tx.shift;
                    printf("\nWrite RTC test reg %02x", tx.shift);
                    break;
                case 0b00110101:
                    write_protect_register = tx.shift;
                    break;
                default:
                    printf("\nUnknown RTC command %02x", cmd);
                    break;
            }
        }
    }
}

void RTC::write_bits(u8 val, u8 write_mask)
{
    // bit 0 - serial data
    // bit 1 - data-clock
    // bit 2 - serial enable (if 1, no data will be shifted, transaction cancelled)
    u8 data, clock, enable;
    if (write_mask & 1) data = val & 1;
    else data = (tx.shift & 0x80) >> 7;

    if (write_mask & 2) clock = (val >> 1) & 1;
    else clock = old_clock_bit;

    if (write_mask & 4) enable = ((val >> 2) & 1) ^ 1;
    else enable = tx.kind > 0;

    // transfers are 8 bits. write is 8 bits out + 8 bits outs
    // read is 8 bits out + 8 bits in
    if (enable) {
        if (clock != old_clock_bit) {
            old_clock_bit = clock;
            if (clock == 1) {
                // Do a transfer.
                // 1 = write, 2 = read
                if ((tx.progress < 8) || (tx.kind == 1)) {
                    // It will be a transfer in
                    tx.shift >>= 1;
                    tx.shift |= (data << 7);
                }
                else { // progress > 8 and it's a read
                    if (tx.kind == 1) {
                        tx.shift <<= 1;
                    }
                }
                tx.progress++;
                // We completed a command...
                if (tx.progress == 8) {
                    cmd = tx.shift;
                    if (cmd & 0x80) {
                        tx.kind = 1; // write
                    }
                    else {
                        tx.kind = 2; // read
                        do_read();
                        // get the data...
                    }
                }
                else if (tx.progress == 16) {
                    // We compelted totality
                    if (tx.kind == 1) { // read
                    }
                    else if (tx.kind == 2) { // write
                        // determine where to commit
                        if (write_protect_register & 0x80) {
                            // If write-protect on, only write to write-protect.
                            if (cmd != 0b00110101) {
                                printf("\nTried to write to write-protected RTC! cyc:%lld", bus->clock.master_cycles);
                            }
                            else {
                                write_protect_register = tx.shift;
                            }
                        }
                        else {
                            finish_write();
                        }
                    }
                    else {
                        printf("\nREACHED END OF UNKNOWN TRANSFER? %02x cyc:%lld", cmd, bus->clock.master_cycles);
                    }
                    tx.kind = 0;
                    tx.shift = 0;
                    tx.progress = 0;
                }
            }
        }
    }
    else {
        // reset transfer
        tx.shift = 0;
        tx.progress = 0;
        tx.kind = 0;
    }
}

}