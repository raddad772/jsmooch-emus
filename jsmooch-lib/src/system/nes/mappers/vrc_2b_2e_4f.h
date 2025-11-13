//
// Created by . on 10/1/24.
//

#pragma once

struct VRC2B_4E_4F : NES_mapper {
    explicit VRC2B_4E_4F(NES_bus *, NES_mappers in_kind);

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
    NES_mappers kind{};
    bool is_vrc4{};
    bool is_vrc2a{};

    struct {
        u32 cycle_mode{};
        u32 enable{};
        u32 enable_after_ack{};
        u32 reload{};
        u32 prescaler=341;
        u32 counter{};
    } irq{};
    struct {
        u32 wram_enabled{};
        u32 latch60{};
        struct {
            u32 banks_swapped{};
        } vrc4{};
        struct {
            u32 banks[8]{};
        } ppu{};
        struct {
            u32 bank80{};
            u32 banka0{};
        } cpu{};
    } io{};
    void remap(bool boot);
    void set_ppu_lo(u32 bank, u32 val);
    void set_ppu_hi(u32 bank, u32 val);
};