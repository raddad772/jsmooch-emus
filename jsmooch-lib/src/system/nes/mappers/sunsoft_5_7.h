//
// Created by . on 10/3/24.
//

#pragma once

#include "mapper.h"
struct NES_bus;
struct NES;

struct sunsoft_5_7 : NES_mapper {
    explicit sunsoft_5_7(NES_bus *, NES_mappers in_kind);

    void writecart(u32 addr, u32 val, u32 &do_write) override;
    u32 readcart(u32 addr, u32 old_val, u32 has_effect, u32 &do_read) override;
    void setcart(NES_cart &cart) override;
    void reset() override;

    void serialize(serialized_state &state) override;
    void deserialize(serialized_state &state) override;

    // Optional ones
    //void a12_watch(u32 addr) override;
    void cpu_cycle() override;
    //float sample_audio() override;
    //u32 PPU_read_override(u32 addr, u32 has_effect);
    //void PPU_write_override(u32 addr, u32 val);

private:
    u32 bank_mask{};
    u32 has_sound{};

    NES_mappers kind{};
    struct {
        u32 counter{};
        u32 output{};
        u32 enabled{};
        u32 counter_enabled{};
    } irq{};
    struct {
        struct {
            u32 banks[4]{};
        } cpu{};
        struct {
            u32 banks[8]{};
        } ppu{};
        u32 reg{};
        u32 wram_enabled{}, wram_mapped{};

        struct {
            u32 reg{};
            u32 write_enabled{};
        } audio{};
    } io{};
    void remap(bool boot);
    void write_reg(u32 val);
    void write_audio_reg(u32 val);
};