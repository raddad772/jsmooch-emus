//
// Created by . on 10/3/24.
//
#pragma once

struct NES_bus;
struct NES;

struct MMC5 : NES_mapper {
    explicit MMC5(NES_bus *);

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
    u32 PPU_read_override(u32 addr, u32 has_effect) override;
    void PPU_write_override(u32 addr, u32 val) override;

private:
    simplebuf8 exram{};

    struct {
        struct {
            u32 sprites_8x16{};
            u32 substitutions_enabled{};
        } ppu{};

        u32 PRG_bank_mode{}, CHR_page_size{};

        u32 PRG_RAM_protect[2]{}, PRG_RAM_protect_mux{};

        u32 exram_mode{};

        u32 upper_CHR_bits{};

        u32 CHR_bank[12]{};
        u32 PRG_bank[5]{};

        u32 multplicand{}, multiplier{};

        u32 fill_tile{}, fill_color{};

        u32 irq_scanline{}, irq_enabled{};
    } io{};
    u32 rendering_sprites{};
    u32 blanking{};

    u32 nametables[4]{};
    struct {
        u32 reads{};
        u32 addr{};
        u32 in_frame{}, scanline{};
        u32 pending{};
    } irq{};
    NES_memmap PPU_map[2][0x4000 / 0x400]{};

    void map_CHR1K(u32 sprites_status, u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly);
    void map_CHR2K(u32 sprites_status, u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly);
    void map_CHR4K(u32 sprites_status, u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly);
    void map_CHR8K(u32 sprites_status, u32 range_start, u32 range_end, simplebuf8 *buf, u32 bank, u32 is_readonly);
    void remap_CHR();
    void remap_PRG();
    void map_PRG(u32 start_addr, u32 end_addr, u32 val);
    void remap(bool boot);
};