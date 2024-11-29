//
// Created by . on 11/27/24.
//

#ifndef JSMOOCH_EMUS_BUILD_H
#define JSMOOCH_EMUS_BUILD_H

#define JSM_WEBGPU
//#define JSM_SDLR3
//#define JSM_OPENGL
//#define JSM_METAL

#include "imgui.h"

#ifdef JSM_WEBGPU
#include "../vendor/myimgui/backends/imgui_impl_glfw.h"
#include "../vendor/myimgui/backends/imgui_impl_wgpu.h"
#endif

#ifdef JSM_OPENGL
#include "../vendor/myimgui/backends/imgui_impl_sdl3.h"
#include "../vendor/myimgui/backends/imgui_impl_opengl3.h"
#endif

#ifdef JSM_METAL
#include "../vendor/myimgui/backends/imgui_impl_sdl3.h"
#include "../vendor/myimgui/backends/imgui_impl_metal.h"
#endif

#ifdef JSM_SDLR3
#include "../vendor/myimgui/backends/imgui_impl_sdl3.h"
#include "../vendor/myimgui/backends/imgui_impl_sdlrenderer3.h"
#endif

#endif //JSMOOCH_EMUS_BUILD_H
