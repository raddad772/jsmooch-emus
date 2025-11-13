//
// Created by . on 9/27/24.
//

#pragma once

#include "../mapper.h"
#include "a12_watcher.h"

struct NES_bus;
struct NES;

struct MMC3b_DXROM : NES_mapper {
    explicit MMC3b_DXROM(NES_bus *, NES_mappers in_kind);

    void writecart(u32 addr, u32 val, u32 &do_write) override;
    u32 readcart(u32 addr, u32 old_val, u32 has_effect, u32 &do_read) override;
    void setcart(NES_cart &cart) override;
    void reset() override;

    void serialize(serialized_state &state) override;
    void deserialize(serialized_state &state) override;

    // Optional ones
    void a12_watch(u32 addr) override;
    //void cpu_cycle() override;
    //float sample_audio() override;
    //u32 PPU_read_override(u32 addr, u32 has_effect);
    //void PPU_write_override(u32 addr, u32 val);

private:
    void remap(bool is_boot);;
    void remap_PPU();

    NES_mappers kind;

    NES_a12_watcher a12_watcher;
    u32 has_IRQ{};
    u32 has_mirroring_control{};
    u32 fourway{};
    struct {
        u32 rC000{};
        u32 bank_select{};
        u32 bank[8]{};
    } regs{};

    struct {
        u32 ROM_bank_mode{};
        u32 CHR_mode{};
        u32 PRG_mode{};
    } status{};

    struct {
        u32 enable{}, reload{};
        i32 counter{};
    } irq{};
};

