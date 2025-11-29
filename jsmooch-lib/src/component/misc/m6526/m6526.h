#pragma once

#include "helpers/int.h"

namespace M6526 {

union pins {
    struct {
        u64 PRA_in : 8;
        u64 PRB_in : 8;
        u64 PRA_out : 8;
        u64 PRB_out : 8;
        u64 PC : 1; // read/write of portB
        u64 FLAG : 1; // "IRQ FLAG
        u64 CNT : 1; // "control"
        u64 IRQ : 1;
        u64 SP : 1; // serial data
    };
    u64 u;
};

struct timer {
    u16 latch{};
    u16 count{};
    u8 out{};
    u8 out_count{};
};

struct core {
    void reset();
    void cycle();
    struct {
        u8 DDRA{}, DDRB{}; // Data Direction regs
        u8 TOD10{}, TODSEC{}, TODMIN{}, TODHR{}; // Today time
        u8 ALARM10{}, ALARMSEC{}, ALARMMIN{}, ALARMHR{};
        u8 SDR{}, serial_data{}, serial_data_count{}, serial_data_ready{}; // Serial data
        union {
            struct {
                u8 TA : 1;
                u8 TB : 1;
                u8 ALRM : 1;
                u8 SP : 1;
                u8 FLG : 1;
                u8 _ : 2;
                u8 S_C : 1;
            };
            u8 u;
        } ICR_MASK{};
        union {
            struct {
                u8 TA : 1;
                u8 TB : 1;
                u8 ALRM : 1;
                u8 SP : 1;
                u8 FLG : 1;
                u8 _ : 2;
                u8 IR : 1;
            };
            u8 u;
        } ICR_DATA{};
        union {
            struct {
                u8 START : 1; // TimerA start
                u8 PBON : 1; // TimerA output on PortB 6
                u8 OUTMODE : 1; // 1=Toggle or 0=pulse output for timer
                u8 RUNMODE: 1; // 0=CONTINUOUS, 1=1 SHOT
                u8 LOAD : 1; // 1=FORCE LOAD (strobe input, bit 4 reads back 0 always)
                u8 INMODE : 1; // 0=timerA counts positive CNT transitions, 0=phi2 pulses
                u8 SPMODE : 1; // 1= serial port out (CNT clocks), 0 = serial port in (external shift clock required)
                u8 TODIN : 1; // 1=50Hz clock, 0=60Hz for Today clock
            };
            u8 u;
        } CRA{};

        union {
            struct {
                u8 START : 1; // TimerB start
                u8 PBON : 1; // TimerB output on PortB 6
                u8 OUTMODE : 1; // 1=Toggle or 0=pulse output for timer
                u8 RUNMODE: 1; // 0=CONTINUOUS, 1=1 SHOT
                u8 LOAD : 1; // 1=FOCE LOAD (strobe input, bit 4 reads back 0 always)
                u8 INMODE : 2; // 0=in clock speed, 1=positive CNT, 2=timer A underflow, 3=timer A underflow while CNT is high
                u8 ALARM : 1; // 1=writing TOD registers sets alarm. 0=sets TOD
            };
            u8 u;
        } CRB{};
    } regs{};
    pins pins{};
    timer timerA{}, timerB{};

    u8 read(u8 addr, u8 old, bool has_effect);
    void write(u8 addr, u8 val);
    u8 read_PRA();
    //u8 read_PRA_3();
    u8 read_PRB(bool has_effect);
    void set_IRQ_pin(u8 val);

    struct {
        void *ptr{};
        u32 device_num{};
        void (*func)(void *ptr, u32 device_num, u8 lvl);
    } update_irq{};

private:
    int PC_count{};
    int FLAG_count{};
    u8 old_FLAG{};
    u8 old_CNT{};
    void write_PRA(u8 val);
    void write_PRB(u8 val);
    void write_DDRA(u8 val);
    void write_DDRB(u8 val);
    void write_CRA(u8 val);
    void write_CRB(u8 val);
    u8 read_ICR(bool has_effect);
    void write_ICR(u8 val);
    void write_SDR(u8 val);

    void tick_A();
    void tick_B();

    void update_IRQs();
};
}