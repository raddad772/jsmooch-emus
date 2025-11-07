#pragma once

#include "full_sys.h"
#include "helpers_c/int.h"
#include "helpers_c/inifile.h"

enum managed_window_kind {
    mwk_main,
    mwk_playback,
    mwk_opts,
    mwk_other,
    mwk_debug_console,
    mwk_debug_log,
    mwk_debug_trace,
    mwk_debug_sound,
    mwk_debug_image,
    mwk_debug_disassembly,
    mwk_debug_events,
    mwk_debug_memory,
    mwk_debug_dbglog
};

struct managed_window {
    u32 enabled{};
    u32 id{};
    enum managed_window_kind kind{};
    char name[200]{};
};

#define MAX_MANAGED_WINDOWS 100
struct managed_windows {
    struct managed_window items[MAX_MANAGED_WINDOWS]{};
    u32 num=0;
};

struct RenderResources;
struct imgui_jsmooch_app {
    struct managed_windows windows{};
    struct managed_window *register_managed_window(u32 id, enum managed_window_kind kind, const char *name, u32 default_enabled);
    void do_setup_onstart();
    int do_setup_before_mainloop();
    void mainloop(ImGuiIO& io);
    void at_end();
    void render_debug_views(ImGuiIO& io, bool update_dasm_scroll);
    void render_memory_view();
    void render_event_view();
    void render_image_views();
    void render_dbglog_views(bool update_dasm_scroll);
    void render_trace_view(bool update_dasm_scroll);
    void render_console_view(bool update_dasm_scroll);

    void render_waveform_view(struct WVIEW &wview, u32 num);
    void render_disassembly_views(bool update_dasm_scroll);
    void render_disassembly_view(struct DVIEW &dview, bool update_dasm_scroll, u32 num);
    void render_dbglog_view(struct DLVIEW &dview, bool update_dasm_scoll);
    void render_window_manager();
#ifdef JSM_SDLR3
    SDL_Renderer *renderer;
    void platform_setup(SDL_Renderer *mrenderer) {renderer = mrenderer; fsys.platform_setup(renderer); };
#endif
#ifdef JSM_WEBGPU
    WGPUSamplerDescriptor nearest_neighbor_sampler_desc;
    struct RenderResources *rr = nullptr;
    WGPUSampler        old_sampler = nullptr;
    WGPUSampler        my_sampler = nullptr;
    void platform_setup(WGPUDevice device) {fsys.platform_setup(device);};
    void setup_wgpu();
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