//
// Created by . on 8/8/24.
//

#ifndef JSMOOCH_EMUS_my_texture_H
#define JSMOOCH_EMUS_my_texture_H

#ifdef JSM_WEBGPU
#include <cstdio>
#include "build.h"
#include "helpers/int.h"
#include "../vendor/myimgui/imgui.h"
#include "../vendor/myimgui/backends/imgui_impl_glfw.h"
#include "../vendor/myimgui/backends/imgui_impl_wgpu.h"


struct my_texture {
    struct {
        WGPUTextureDescriptor desc{};
        WGPUTexture item{};
    } tex{};
    struct {
        WGPUTextureViewDescriptor desc{};
        WGPUTextureView item{};
    } view{};
    u32 height{}, width{};
    u32 is_good{};
    ImVec2 uv0{}, uv1{};
    ImVec2 sz_for_display{};
    ImVec2 zoom_sz_for_display(float zoom) { return {sz_for_display.x * zoom, sz_for_display.y * zoom}; }
    WGPUDevice wgpu_device{};


    char store_label[500]{};

    ~my_texture();

    void setup(WGPUDevice device, const char *label, u32 width, u32 height);
    void upload_data(void *source_ptr, size_t sz, u32 source_width, u32 source_height);
    [[nodiscard]] ImTextureID for_image() const { return (ImTextureID)view.item; }
};

#endif
#endif //JSMOOCH_EMUS_my_texture_H
