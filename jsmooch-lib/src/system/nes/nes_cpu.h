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
    enum NES_controller_devices kind=NES_NONE;
    void *device{};
    u32 data();
    void latch(u32 what);
};

struct r2A03 {
    explicit r2A03(struct NES *nes);
    M6502 cpu;
    NES *nes;

    void reset();
    void notify_IRQ(u32 level, u32 from);
    void notify_NMI(u32 level);
    void run_cycle();
    u32 read_reg(u32 addr, u32 val, u32 has_effect);
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

    struct NES_controllerport controller_port1{};
    struct NES_controllerport controller_port2{};
    struct NES_joypad joypad1{};
    struct NES_joypad joypad2{};
};

void r2A03_init(struct r2A03*, struct NES* nes);
void r2A03_notify_NMI(struct r2A03*, u32 level);
void r2A03_notify_IRQ(struct r2A03*, u32 level, u32 from);
void r2A03_reset(struct r2A03*);
void r2A03_run_cycle(struct r2A03*);
void NES_bus_CPU_write_reg(struct NES* nes, u32 addr, u32 val);
u32 NES_bus_CPU_read_reg(struct NES* nes, u32 addr, u32 val, u32 has_effect);

void NES_controllerport_init(struct NES_controllerport*);

#endif //JSMOOCH_EMUS_NES_CPU_H
