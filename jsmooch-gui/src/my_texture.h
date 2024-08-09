//
// Created by . on 8/8/24.
//

#ifndef JSMOOCH_EMUS_MY_TEXTURE_H
#define JSMOOCH_EMUS_MY_TEXTURE_H

#include "helpers/int.h"
#include "../vendor/myimgui/imgui.h"
#include "../vendor/myimgui/backends/imgui_impl_glfw.h"
#include "../vendor/myimgui/backends/imgui_impl_wgpu.h"

struct my_texture {
    struct {
        WGPUTextureDescriptor desc;
        WGPUTexture item;
    } tex;
    struct {
        WGPUTextureViewDescriptor desc;
        WGPUTextureView item;
    } view;
    u32 height, width;
    WGPUDevice wgpu_device;

    my_texture() { tex.desc = {}; tex.item = nullptr;   view.desc = {}; view.item = nullptr; height = width = 0; wgpu_device = nullptr; }
    ~my_texture() {
        if (tex.item) {
            wgpuTextureDestroy(tex.item);
            wgpuTextureRelease(tex.item);
            tex.item = nullptr;
        }
        if (view.item) {
            wgpuTextureViewRelease(view.item);
            view.item = nullptr;
        }
    }
    void setup(WGPUDevice device, const char *label, u32 width, u32 height);
    void upload_data(void *source_ptr, size_t sz, u32 source_width, u32 source_height);

    [[nodiscard]] WGPUTextureView for_image() const { return view.item; }
};


#endif //JSMOOCH_EMUS_MY_TEXTURE_H
