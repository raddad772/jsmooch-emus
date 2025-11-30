#include <cstdio>
#include <cmath>

#include "helpers/int.h"
#include "c64_vic2_color.h"
namespace VIC2::color::pepto {

typedef struct {
    float u, v, y;
} components;

static constexpr int levels[2][16] = {
    { 0, 32, 10, 20, 12, 16, 8, 24, 12, 8, 16, 10, 15, 24, 15, 20 },
    { 0, 32, 8, 24, 16, 16, 8, 8, 16, 8, 16, 8, 16, 24, 16, 24 }
};

static constexpr int angles[16] = {
    0,      /* 0x0 black/none */
    0,      /* 0x1 white */
    4,      /* 0x2 red */
    4 + 8,  /* 0x3 cyan */
    2,      /* 0x4 purple */
    2 + 8,  /* 0x5 green */
    7 + 8,  /* 0x6 blue */
    7,      /* 0x7 yellow */
    5,      /* 0x8 orange */
    6,      /* 0x9 brown */
    4,      /* 0xa light red */
    4,      /* 0xb dark grey */
    0,      /* 0xc mid grey */
    2 + 8,  /* 0xd light green */
    7 + 8,  /* 0xe light blue */
    0       /* 0xf light grey / cyan pair */
};

void compose(int index, int revision_select, float brightness, float contrast_percent, float saturation_percent, components &c) {
    float constexpr screen = 1.0f / 5.0f;
    /* Copy levels based on revision */
    int level = levels[revision_select][index];

    /* normalize inputs like the JS model */
    brightness -= 50.0f;              /* JS: brightness -= 50 */
    float contrast = contrast_percent / 100.0f; /* JS: contrast /= 100 */
    float saturation = saturation_percent * (1.0f - screen); /* JS: saturation *= 1 - screen */

    /* if we have chroma angle for this index */
    int ang = angles[index];
    if (ang) {
        float sector = 360.0f / 16.0f;
        float origin = sector / 2.0f;
        float angle_deg = (origin + ang * sector);
        float angle = angle_deg * (M_PI / 180.0f);
        c.u = cos(angle) * saturation;
        c.v = sin(angle) * saturation;
    } else {
        c.u = 0.0f;
        c.v = 0.0f;
    }

    c.y = 8.0f * static_cast<float>(level) + brightness;

    /* multiply each component by (contrast + screen) as the JS does */
    float factor = contrast + screen;
    c.u *= factor;
    c.v *= factor;
    c.y *= factor;
}

void convert_yuv_to_rgb(const components &comp, u32 &out_r, u32 &out_g, u32 &out_b) {
    float r = comp.y + 1.140f * comp.v;
    float g = comp.y - 0.396f * comp.u - 0.581f * comp.v;
    float b = comp.y + 2.029f * comp.u;

    /* clamp to [0,255] before gamma ops like JS */
    if (r < 0.0f) r = 0.0f;
    if (r > 255.0f) r = 255.0f;
    if (g < 0.0f) r = 0.0f;
    if (g > 255.0f) r = 255.0f;
    if (b < 0.0f) r = 0.0f;
    if (b > 255.0f) r = 255.0f;

    float vals[3] = {r, g, b};

    const float source = 2.8; /* PAL gamma in Pepto code, also works for NTSC */
    const float target = 2.2; /* sRGB gamma */

    for (int i = 0; i < 3; ++i) {
        /* replicate: Math.pow(255,1 - source) * Math.pow(color[i], source) */
        float v1 = pow(255.0f, 1.0f - source) * pow(vals[i], source);
        /* then Math.pow(255,1 - 1/target) * Math.pow(v1, 1/target) */
        float v2 = pow(255.0f, 1.0f - 1.0f / target) * pow(v1, 1.0f / target);
        /* clamp again and round */
        if (v2 < 0.0f) v2 = 0.0f;
        if (v2 > 255.0f) v2 = 255.0f;
        vals[i] = v2;
    }

    out_r = static_cast<u32>(lrint(vals[0]));
    out_g = static_cast<u32>(lrint(vals[1]));
    out_b = static_cast<u32>(lrint(vals[2]));
}

u32 get_abgr(int index, int revision, float brightness, float contrast, float saturation) {
    components c;
    compose(index, revision, brightness, contrast, saturation, c);
    u32 r,g,b;
    convert_yuv_to_rgb(c, r, g, b);
    u32 o = 0xFF000000;
    o |= b << 16;
    o |= g << 8;
    o |= r << 0;
    return o;
}

}