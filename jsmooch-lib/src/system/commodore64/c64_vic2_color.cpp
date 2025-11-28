//
// Created by . on 11/26/25.
//

#include <cmath>
#include "c64_vic2_color.h"

namespace VIC2::color {

constexpr float levels[2][16] =
{
    {
        0.0f, 32.0f, 8.0f, 24.0f,
        16.0f, 16.0f, 8.0f, 24.0f,
        16.0f, 8.0f, 16.0f, 8.0f,
        16.0f, 24.0f, 16.0f, 24.0f
    },
    {
        0.0f, 32.0f, 10.0f, 20.0f, // 0-3
        12.0f, 16.0f, 8.0f, 24.0f, // 4-7
        12.0f, 8.0f, 16.0f, 10.0f, // 8-b
        15.0f, 24.0f, 15.0f, 20.0f
    }
};

constexpr float angles[] = {
    0.0f, 0.0f, 4.0f, 12.0f,
    2.0f, 10.0f, 15.0f, 7.0f,
    5.0f, 6.0f, 4.0f, 0.0f,
    0.0f, 10.0f, 15.0f, 0.0f
};

#define Y 0
#define U 1
#define V 2
static void compose(int index, int revision, float brightness, float contrast, float saturation, float components[3])
{
    // constants
    constexpr float sector = 27.0f; // 360.0/16
    constexpr float origin = sector/2;
    constexpr float radian = M_PI/180.0f;
    constexpr float screen = 1.0f/5.0f;

    // normalize
    brightness -= 50.0f;
    contrast /= 100.0f;
    saturation *= 1.0f - screen;

    // construct
    components[0] = components[1] = 0.0f;

    if (angles[index] != 0.0f) {
        float angle = (origin + angles[index] * sector) * radian;

        components[U] = cosf(angle) * saturation;
        components[V] = sinf(angle) * saturation;
    }

    components[Y] = 8.0f * levels[revision][index] + brightness;

    for (u32 i = 0; i < 3; i++)
    {
        components[i] *= contrast + screen;
    }
}

static float max(float a, float b) {
    return a > b ? a : b;
}

static float min(float a, float b) {
    return a < b ? a : b;
}

static void yuv_to_rgb(float components[3], float color[3]) {
    // matrix transformation
    color[0] = components[Y] + 1.140f * components[V];
    color[1] = components[Y] - 0.396f * components[U] - 0.581f * components[V];
    color[2] = components[Y] + 2.029f * components[U];

    // gamma correction
    float source = 2.8f;
    float target = 2.2f; // Source says this is for PAL but it hols true for NTSC as well

    for (int i = 0; i < 3; i++) {
        color[i] = max(min(color[i], 255.0f), 0.0f);

        color[i] = powf(255.0f, 1.0f - source) * powf(color[i], source);
        color[i] = powf(255.0f, 1.0f - 1.0f/target) * powf(color[i], 1.0f/target);

        color[i] = roundf(color[i]);
    }
}

u32 calculate_color(int index, int revision, float brightness, float contrast, float saturation) {
    u32 color = 0xFF000000;
    float components[3];
    float acolor[3];
    compose(index, revision, brightness, contrast, saturation, components);
    yuv_to_rgb(components, acolor);
    color |= static_cast<u32>(acolor[2]) << 0;
    color |= static_cast<u32>(acolor[1]) << 8;
    color |= static_cast<u32>(acolor[0]) << 16;
    return color;
}

}