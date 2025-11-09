//
// Created by Dave on 2/5/2024.
//

#ifndef JSMOOCH_EMUS_NES_CPU_H
#define JSMOOCH_EMUS_NES_CPU_H

#include "component/cpu/m6502/m6502.h"
#include "helpers/int.h"
#include "helpers/irq_multiplexer.h"
#include "component/controller/nes/nes_joypad.h"

enum NES_controller_devices {
    NES_NONE,
    NES_JOYPAD
};

struct NES_controllerport {
    NES_controller_devices kind=NES_NONE;
    void *device{};
    u32 data() const;
    void latch(u32 what) const;
};

struct r2A03 {
    explicit r2A03(struct NES *nes);
    M6502 cpu;
    NES *nes;

    void reset();
    void notify_IRQ(u32 level, u32 from);
    void notify_NMI(u32 level);
    void run_cycle();
    u32 read_reg(u32 addr, u32 val, u32 has_effect) const;
    void write_reg(u32 addr, u32 val);

    //struct NES_APU apu;

    u32 tracing{};
    u8 open_bus{};

    IRQ_multiplexer irq{};

    struct {
        struct {
            u32 addr{};
            u32 running{};
            u32 bytes_left{};
            u32 step{};
        } dma{};
    } io{};

    NES_controllerport controller_port1{};
    NES_controllerport controller_port2{};
    NES_joypad joypad1{0};
    NES_joypad joypad2{1};

    void serialize(struct serialized_state &state);
    void deserialize(struct serialized_state &state);
};

#endif //JSMOOCH_EMUS_NES_CPU_H
