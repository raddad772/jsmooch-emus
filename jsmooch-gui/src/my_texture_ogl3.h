//
// Created by . on 8/8/24.
//

#ifndef JSMOOCH_EMUS_MY_TEXTURE_OGL3_H
#define JSMOOCH_EMUS_MY_TEXTURE_OGL3_H

#include <stdio.h>

#include "build.h"


#ifdef JSM_OPENGL

#include "helpers_c/int.h"
#include "../vendor/myimgui/imgui.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_opengl.h"

struct my_texture  {
    struct {
        //WGPUTextureDescriptor desc{};
        //WGPUTexture item{};
        GLuint item;
    } tex{};
    struct {
        //WGPUTextureViewDescriptor desc{};
        //WGPUTextureView item{};
    } view{};
    u32 height{}, width{};
    u32 is_good{};
    ImVec2 uv0{}, uv1{};
    ImVec2 sz_for_display{};
    ImVec2 zoom_sz_for_display(float zoom) { return {sz_for_display.x * zoom, sz_for_display.y * zoom}; }
    [[nodiscard]] ImTextureID for_image() { return tex.item; }


    char store_label[500]{};

    ~my_texture();
    void setup(const char *label, u32 width, u32 height);
    void upload_data(void *source_ptr, size_t sz, u32 source_width, u32 source_height);
};

#endif
#endif //JSMOOCH_EMUS_my_texture_H
