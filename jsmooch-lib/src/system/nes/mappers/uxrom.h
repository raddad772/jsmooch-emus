//
// Created by . on 9/29/24.
//

#pragma once

struct UXROM : NES_mapper {
    explicit UXROM(NES_bus *);

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
    struct {
        u32 bank_num{};
    } io{};
    void remap(bool is_boot);
};
