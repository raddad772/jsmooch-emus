//
// Created by . on 9/27/24.
//

#pragma once

#include "helpers/int.h"
#include "helpers/serialize/serialize.h"
#include "../nes_cart.h"

// NES_bus takes care of mappers!

typedef u32 (*mirror_ppu_t)(u32);

enum NES_mappers {
    NESM_UNKNOWN,
    NESM_NROM,
    NESM_MMC1,
    NESM_UXROM,
    NESM_CNROM,
    NESM_MMC3b,
    NESM_AXROM,
    NESM_VRC4E_4F,
    NESM_DXROM,
    NESM_SUNSOFT_5b,
    NESM_SUNSOFT_7,
    NESM_JF11_JF14,
    NESM_GNROM,
    NESM_COLOR_DREAMS,
    NESM_MMC5
};

struct NES;
struct NES_bus;
struct NES_cart;

struct NES_mapper {
    virtual ~NES_mapper() = default;
    explicit NES_mapper(NES_bus *bus) : bus(bus) {};
    virtual void writecart(u32 addr, u32 val, u32 &do_write)=0;
    virtual u32 readcart(u32 addr, u32 old_val, u32 has_effect, u32 &do_read)=0;
    virtual void setcart(NES_cart &cart)=0;
    virtual void reset()=0;

    virtual void serialize(serialized_state &state)=0;
    virtual void deserialize(serialized_state &state)=0;

    virtual void a12_watch(u32 addr){}
    virtual void cpu_cycle(){}
    virtual float sample_audio(){return 0.0f;}
    virtual u32 PPU_read_override(u32 addr, u32 has_effect){return 0;}
    virtual void PPU_write_override(u32 addr, u32 val){}

    bool overrides_PPU{};
    NES_bus *bus{};
};

