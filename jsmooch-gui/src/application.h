#pragma once

#include "full_sys.h"
#include "helpers/int.h"
#include "helpers/inifile.h"

struct imgui_jsmooch_app {
    void do_setup_onstart();
    int do_setup_before_mainloop();
    void mainloop(ImGuiIO& io);
    void at_end();

#ifdef JSM_SDLR3
    SDL_Renderer *renderer;
    void platform_setup(SDL_Renderer *mrenderer) {renderer = mrenderer; fsys.platform_setup(renderer); };
#endif
#ifdef JSM_WEBGPU
    void platform_setup(WGPUDevice device) {fsys.platform_setup(device);};
#endif

    int done_break = 0;
    bool last_frame_was_whole = false;
    bool playing = true;
    bool done = false;
    full_system fsys;
    enum jsm_systems which;
    struct inifile ini;

    char BIOS_BASE_PATH[500];
    char ROM_BASE_PATH[500];
    char *debugger_cols[3];
    constexpr static size_t debugger_col_sizes[3] = { 10 * 1024, 100 * 1024, 500 * 1024 };

};