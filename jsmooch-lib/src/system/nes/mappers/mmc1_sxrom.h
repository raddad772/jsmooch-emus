//
// Created by . on 10/1/24.
//
#pragma once

struct NES_bus;
struct NES;

struct SXROM : NES_mapper {
    explicit SXROM(NES_bus *);

    void writecart(u32 addr, u32 val, u32 &do_write) override;
    u32 readcart(u32 addr, u32 old_val, u32 has_effect, u32 &do_read) override;
    void setcart(NES_cart &cart) override;
    void reset() override;

    void serialize(serialized_state &state) override;
    void deserialize(serialized_state &state) override;

    // Optional ones
    //void a12_watch(u32 addr) override;
    //void cpu_cycle() override;
    //float sample_audio() override;
    //u32 PPU_read_override(u32 addr, u32 has_effect);
    //void PPU_write_override(u32 addr, u32 val);

private:
    void remap(u32 boot);
    u64 last_cycle_write{};
    struct {
        u32 shift_pos{};
        u32 shift_value{};
        u32 ppu_bank00{};
        u32 ppu_bank10{};
        u32 bank{};
        u32 ctrl{};
        u32 prg_bank_mode{};
        u32 chr_bank_mode{};
    } io{};
};

