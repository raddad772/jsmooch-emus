//
// Created by Dave on 2/16/2024.
//

#ifndef JSMOOCH_EMUS_HOLLY_H
#define JSMOOCH_EMUS_HOLLY_H

#include "dreamcast.h"


enum DCPolyKind {
    DCP_type0,
    DCP_type1,
    DCP_type2,
    DCP_type3,
    DCP_type4,
    DCP_sprite
};

enum DCVertexParamKind {
    DCVP0 = 0,
    DCVP1 = 1,
    DCVP2 = 2,
    DCVP3 = 3,
    DCVP4 = 4,
    DCVP5 = 5,
    DCVP6 = 6,
    DCVP7 = 7,
    DCVP8 = 8,
    DCVP9 = 9,
    DCVP10 = 10,
    DCVP11 = 11,
    DCVP12 = 12,
    DCVP13 = 13,
    DCVP14 = 14,
    DCVPSP0 = 16,
    DCVPSP1 = 17
};

union HOLLY_MVI {
    struct {
        u32 : 27;
        u32 culling_mode : 2;
        u32 volume_instruction : 3;
    };
    u32 u;
};

union HOLLY_PCW {
    struct {
        u32 uv_is_16bit: 1;
        u32 gouraud: 1;
        u32 offset: 1;
        u32 texture: 1;
        u32 col_type: 2;
        u32 volume: 1;
        u32 shadow: 1;
        u32 : 8;
        u32 user_clip: 2;
        u32 strip_len: 2;
        u32 : 3;
        u32 group_en: 1;
        u32 list_type: 3;
        u32 : 1;
        u32 end_of_strip: 1;
        u32 para_type: 3;
    };
    u32 u;
};

union HOLLY_ISP_TSP_IWORD {
    struct {
        u32 : 20;
        u32 dcalc_ctrl : 1;
        u32 cache_bypass : 1;
        u32 uv_16bit : 1;
        u32 gouraud : 1;
        u32 offset : 1;
        u32 texture : 1;
        u32 z_write_disable : 1;
        u32 culling_mode : 2;
        u32 depth_compare_mode : 3;
    };
    u32 u;
};

enum HOLLY_ALPHA_INS {
    zero = 0,
    one = 1,
    other_color = 2,
    inverse_other_color = 3,
    source_alpha = 4,
    inverse_source_alpha = 5,
    dest_alpha = 6,
    inverse_dest_alpha = 7
};


enum HOLLY_TEX_PIXEL_FORMAT {
    ARGB1555 = 0, RGB565, ARGB4444, YUV422,
    bump_map, palette4BPP, palette8BPP, res
};

enum HOLLY_FOG_CTRL {
    LUT = 0, vertex = 1, none = 2, LUT2 = 3
};

enum HOLLY_TEXTURE_SHADE_INS {
    decal = 0, modulate, decal_alpha, modulate_alpha
};

struct DCPackedColor {
    u8 b;
    u8 g;
    u8 r;
    u8 a;
} __attribute__((packed));

union HOLLY_TSP_IWORD {
    struct {
        u32 texture_v_size : 3;
        u32 texture_u_size : 3;
        u32 texture_shading_instruction : 2;
        u32 mapmap_d_adjust : 4;
        u32 supersample_texture : 1;
        u32 filter_mode : 2;
        u32 clamp_uv : 2;
        u32 flip_uv : 2;
        u32 ignore_texture_alpha : 1;
        u32 use_alpha : 1;
        u32 color_clamp : 1;
        u32 fog_control : 2;
        u32 dst_select : 1;
        u32 src_select : 1;
        u32 dst_alpha_instr : 3;
        u32 src_alpha_instr : 3;
    };
    u32 u;
};

union HOLLY_TEX_CTRL {
    struct {
        u32 address : 21;
        u32  : 4;
        u32 stride_select : 1;
        u32 scan_order : 1;
        u32 pixel_format : 3;
        u32 vq_compressed : 1;
        u32 mip_mapped : 1;
    };
    u32 u;
};

struct DCUV16 {
    u16 u, v;
} __attribute__((packed));

struct DCPoly0 { // packed, floating color
    union HOLLY_PCW parameter_control_word;
    union HOLLY_ISP_TSP_IWORD isp_tsp_word;
    union HOLLY_TSP_IWORD tsp_word;
    union HOLLY_TEX_CTRL texture_ctrl;
    u64 empty;
    u32 data_size;
    u32 next_address;
} __attribute__((packed));

struct DCPoly1 { // intensity no offset
    union HOLLY_PCW parameter_control_word;
    union HOLLY_ISP_TSP_IWORD isp_tsp_word;
    union HOLLY_TSP_IWORD tsp_word;
    union HOLLY_TEX_CTRL texture_ctrl;
    struct {
        float a, r, g, b;
    } face __attribute__((packed));
} __attribute__((packed));

struct DCPoly2 { // intensity with offset
    union HOLLY_PCW parameter_control_word;
    union HOLLY_ISP_TSP_IWORD isp_tsp_word;
    union HOLLY_TSP_IWORD tsp_word;
    union HOLLY_TEX_CTRL texture_ctrl;
    u64 res;
    u32 data_size;
    u32 next_address;
    struct { float a, r, g, b; } face_color __attribute__((packed));
    struct { float a, r, g, b; } face_offset_color __attribute__((packed));
} __attribute__((packed));

struct DCPoly3 { // 2 volume packed color
    union HOLLY_PCW parameter_control_word;
    union HOLLY_ISP_TSP_IWORD isp_tsp_word;
    union HOLLY_TSP_IWORD tsp_word_0;
    union HOLLY_TEX_CTRL texture_ctrl_0;
    union HOLLY_TSP_IWORD tsp_word_1;
    union HOLLY_TEX_CTRL texture_ctrl_1;
    u32 data_size;
    u32 next_address;
} __attribute__((packed));

struct DCPoly4 { // 2 volume intensity
    union HOLLY_PCW parameter_control_word;
    union HOLLY_ISP_TSP_IWORD isp_tsp_word;
    union HOLLY_TSP_IWORD tsp_word_0;
    union HOLLY_TEX_CTRL texture_ctrl_0;
    union HOLLY_TSP_IWORD tsp_word_1;
    union HOLLY_TEX_CTRL texture_ctrl_1;
    u32 data_size;
    u32 next_address;
    struct { float a, r, g, b; } face_color __attribute__((packed));
    struct { float a, r, g, b; } face_offset_color __attribute__((packed));
} __attribute__((packed));

// Packed Color, Non-Textured
struct DCVertexP0 {
    union HOLLY_PCW parameter_control_word;
    float x, y, z;
    double _0;
    float base_color;
    float _1;
} __attribute__((packed));

struct DCVertexP1 {
    union HOLLY_PCW parameter_control_word;
    float x, y, z;
    float a, r, g, b;
} __attribute__((packed));

// Non-Textured, Intensity
struct DCVertexP2 {
    union HOLLY_PCW parameter_control_word;
    float x, y, z;
    double _0;
    float base_intensity;
    float _1;
} __attribute__((packed));

// Packed Color, Textured 32bit UV
struct DCVertexP3 {
    union HOLLY_PCW parameter_control_word;
    float x, y, z;
    float u, v;
    float base_color, offset_color;
} __attribute__((packed));

// Packed Color, Textured 16bit UV
struct DCVertexP4 {
    union HOLLY_PCW parameter_control_word;
    float x, y, z;
    struct DCUV16 uv;
    float _i;
    float base_color, offset_color;
} __attribute__((packed));

// Floating Color, Textured
struct DCVertexP5 {
    union HOLLY_PCW parameter_control_word;
    float x, y, z;
    float u, v;
    double _i;
    float base_a, base_r, base_g, base_b;
    float offset_a, offset_r, offset_g, offset_b;
} __attribute__((packed));

// Floating Color, Textured 16bit UV
struct DCVertexP6 {
    union HOLLY_PCW parameter_control_word;
    float x, y, z;
    struct DCUV16 uv;
    float _i0, _i1, _i2;
    float base_a, base_r, base_g, base_b;
    float offset_a, offset_r, offset_g, offset_b;
} __attribute__((packed));

// Intensity, Textured 32bit UV
struct DCVertexP7 {
    union HOLLY_PCW parameter_control_word;
    float x, y, z;
    float u, v;
    float base_intensity;
    float offset_intensity;
} __attribute__((packed));

// Intensity, Textured 16bit UV
struct DCVertexP8 {
    union HOLLY_PCW parameter_control_word;
    float x, y, z;
    struct DCUV16 uv;
    float _i0;
    float base_intensity;
    float offset_intensity;
} __attribute__((packed));

// Non-Textured, Packed Color, with Two Volumes
struct DCVertexP9 {
    union HOLLY_PCW parameter_control_word;
    float x, y, z;
    struct DCPackedColor base_0;
    struct DCPackedColor base_1;
    float base_color;
    double _i0;
} __attribute__((packed));

// Non-Textured, Intensity, with Two Volumes
struct DCVertexP10 {
    union HOLLY_PCW parameter_control_word;
    float x, y, z;
    float base_intensity_0;
    float base_intensity_1;
    double _i0;
} __attribute__((packed));

// Textured, Packed Color, with Two Volumes
struct DCVertexP11 {
    union HOLLY_PCW parameter_control_word;
    float x, y, z;
    float u0, v0;
    struct DCPackedColor base_color_0;
    struct DCPackedColor offset_color_0;
    float u1, v1;
    struct DCPackedColor base_color_1;
    struct DCPackedColor offset_color_1;
    u64 _i0, _i1;
} __attribute__((packed));

// Textured, Packed Color, 16bit UV, with Two Volumes
struct DCVertexP12 {
    union HOLLY_PCW parameter_control_word;
    float x, y, z;
    struct DCUV16 uv_0;
    float _i0;
    struct DCPackedColor base_color_0;
    struct DCPackedColor offset_color_0;
    struct DCUV16 uv_1;
    float _i1;
    struct DCPackedColor base_color_1;
    struct DCPackedColor offset_color_1;
    float _i2;
} __attribute__((packed));

// Textured, Intensity, with Two Volumes
struct DCVertexP13 {
    union HOLLY_PCW parameter_control_word;
    float x, y, z;
    float u0, v0;
    float base_intensity_0;
    float offset_intensity_0;
    float u1, v1;
    float base_intensity_1;
    float offset_intensity_1;
    float _i[4];
} __attribute__((packed));

// Textured, Intensity, with Two Volumes
struct DCVertexP14 {
    union HOLLY_PCW parameter_control_word;
    float x, y, z;
    struct DCUV16 uv_0;
    float _i0;
    float base_intensity_0;
    float offset_intensity_0;
    struct DCUV16 uv_1;
    float _i1;
    float base_intensity_1;
    float offset_intensity_1;
    float _i[4];
} __attribute__((packed));

struct DCVertexS0 {
    union HOLLY_PCW parameter_control_word;
    float ax, ay, az;
    float bx, by, bz;
    float cx, cy, cz;
    float dx, dy;
    float unused[4];
} __attribute__((packed));

struct DCVertexS1 {
    union HOLLY_PCW parameter_control_word;
    float ax, ay, az;
    float bx, by, bz;
    float cx, cy, cz;
    float dx, dy;
    float unused;
    struct DCUV16 auv, buv, cuv;
} __attribute__((packed));


struct DCUserTileClip {
    union HOLLY_PCW PCW;
    u64 ign1;
    u32 ign2;
    u32 user_clip_x_min;
    u32 user_clip_y_min;
    u32 user_clip_x_max;
    u32 user_clip_y_max;
}__attribute__((packed));

struct DCpacked_color {
    u8 b, g, r, a;
} __attribute__((packed));

struct DCSprite { //
    union HOLLY_PCW parameter_control_word;
    union HOLLY_ISP_TSP_IWORD isp_tsp_word;
    union HOLLY_TSP_IWORD tsp_word;
    union HOLLY_TEX_CTRL texture_ctrl;
    struct DCpacked_color base_color;
    struct DCpacked_color offset_color;
    u32 data_size;
    u32 next_address;
} __attribute__((packed));

struct DCPoly {
    u32 valid;
    enum DCPolyKind kind;
    u8 data[64];
};

struct DCVertexParam {
    u32 valid;
    enum DCVertexParamKind kind;
    u8 data[64];
};

struct DCModifierVolume {
    u32 valid;
    union HOLLY_PCW parameter_control_word;
    union HOLLY_MVI instructions;
    u32 first_triangle_index;
    u32 triangle_count;
};

struct DCModifierVolumeParameter {
    union HOLLY_PCW parameter_control_word;
    float ax;
    float ay;
    float az;
    float bx;
    float by;
    float bz;
    float cx;
    float cy;
    float cz;
} __attribute__((packed));



enum DCUserClipUsage {
    DCUCU_disable = 0,
    DCUCU_reserved = 1,
    DCUCU_inside_enabled = 2,
    DCUCU_outside_enabled = 3
};

struct DCUserTileClipInfo {
    u32 valid;
    enum DCUserClipUsage user_clip_usage;
    u32 x; // In pixels
    u32 y;
    u32 width;
    u32 height;
};


struct DCVertexStrip {
    struct DCPoly polygon;
    struct DCUserTileClipInfo user_clip;
    u32 verter_parameter_index;
    u32 verter_parameter_count;
};

void holly_init(struct DC*this);
void holly_delete(struct DC* this);

void holly_write(struct DC* this, u32 addr, u32 val, u32* success);
u64 holly_read(struct DC* this, u32 addr, u32* success);
void holly_reset(struct DC* this);
void DC_recalc_frame_timing(struct DC* this);
void holly_vblank_in(struct DC* this);
void holly_vblank_out(struct DC* this);

enum holly_interrupt_masks;
void holly_raise_interrupt(struct DC* this, enum holly_interrupt_masks irq_num, i64 delay);
void holly_lower_interrupt(struct DC* this, enum holly_interrupt_masks irq_num);
void holly_eval_interrupt(struct DC* this, enum holly_interrupt_masks irq_num, u32 is_true);
void holly_recalc_interrupts(struct DC* this);

void holly_TA_FIFO_DMA(struct DC* this, u32 src_addr, u32 tx_len, void *src, u32 src_len);

#endif //JSMOOCH_EMUS_HOLLY_H
