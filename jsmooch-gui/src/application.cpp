// Dear ImGui: standalone example application for using GLFW + WebGPU
// - Emscripten is supported for publishing on web. See https://emscripten.org.
// - Dawn is used as a WebGPU implementation on desktop.

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp
#include <cstdio>

#include "../vendor/myimgui/imgui.h"
#include "../vendor/myimgui/backends/imgui_impl_sdl3.h"
#include "../vendor/myimgui/backends/imgui_impl_opengl3.h"
#include <SDL3/SDL.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL3/SDL_opengles2.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif


#include "helpers/debug.h"
#include "keymap_translate.h"
#include "my_texture_ogl3.h"
#include "full_sys.h"
#include "helpers/inifile.h"

//#define STOPAFTERAWHILE


#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

// Forward declarations
static void load_inifile(struct inifile* ini)
{
    inifile_init(ini);
    char OUTPATH[500] = {};
    construct_path_with_home(OUTPATH, sizeof(OUTPATH), "dev/jsmooch.ini");

    inifile_load(ini, OUTPATH);

}

static void update_input(struct full_system* fsys, ImGuiIO& io) {
    //if (io.WantCaptureKeyboard) {
        // Handle KB
        if (fsys->io.keyboard.vec) {
            auto *pio = (struct physical_io_device *)cpg(fsys->io.keyboard);
            if (pio->connected && pio->enabled) {
                struct JSM_KEYBOARD *kbd = &pio->keyboard;
                for (u32 i = 0; i < kbd->num_keys; i++) {
                    kbd->key_states[i] = ImGui::IsKeyDown(jk_to_imgui(kbd->key_defs[i]));
                }
            }
        }
        // Handle controller 1
        if (fsys->io.controller1.vec) {
            auto *pio = (struct physical_io_device *)cpg(fsys->io.controller1);
            if (pio->connected && pio->enabled) {
                struct JSM_CONTROLLER *ctr = &pio->controller;
                for (u32 i = 0; i < cvec_len(&ctr->digital_buttons); i++) {
                    struct HID_digital_button *db = (struct HID_digital_button *)cvec_get(&ctr->digital_buttons, i);
                    db->state = ImGui::IsKeyDown(jk_to_imgui(db->common_id));
                }
            }
        }
    //}
}

static void render_emu_window(struct full_system &fsys, ImGuiIO& io)
{
    if (ImGui::Begin(fsys.sys->label)) {
        ImGui::Checkbox("2x Zoom", &fsys.output.zoom);
        ImGui::SameLine();
        ImGui::Checkbox("Hide Overscan", &fsys.output.hide_overscan);
        ImGui::Image(fsys.output.backbuffer_texture.for_image(), fsys.output_size(), fsys.output_uv0(), fsys.output_uv1());
        ImGui::Text("FPS: %.1f", io.Framerate);
    }
    ImGui::End();
}

static void render_event_view(struct full_system &fsys)
{
    if (fsys.events.view) {
        if (ImGui::Begin("Event Viewer")) {
            static bool ozoom = true;
            ImGui::Checkbox("2x Zoom", &ozoom);
            fsys.events_view_present();
            ImGui::Image(fsys.events.texture.for_image(), fsys.events.texture.zoom_sz_for_display(ozoom ? 2 : 1),
                         fsys.events.texture.uv0, fsys.events.texture.uv1, {1.0, 1.0, 1.0, 1.0}, {1.0, 1.0, 1.0, 1.0});
            static bool things_open[50];
            static float color_edits[50 * 10][3];
            u32 idx = 0;
            for (u32 cati = 0; cati < cvec_len(&fsys.events.view->categories); cati++) {
                auto *cat = (struct event_category *) cvec_get(&fsys.events.view->categories, cati);
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
                if (ImGui::TreeNodeEx(cat->name, flags)) {
                    for (u32 evi = 0; evi < cvec_len(&fsys.events.view->events); evi++) {
                        auto *event = (struct debugger_event *) cvec_get(&fsys.events.view->events, evi);
                        if (event->category_id == cat->id) {
                            color_edits[idx][0] = (float) (event->color & 0xFF) / 255.0;
                            color_edits[idx][1] = (float) ((event->color >> 8) & 0xFF) / 255.0;
                            color_edits[idx][2] = (float) ((event->color >> 16) & 0xFF) / 255.0;
                            ImGui::ColorEdit3(event->name, color_edits[idx], ImGuiColorEditFlags_NoInputs);
                            idx++;
                        }
                    }
                    ImGui::TreePop();
                }
            }
        }
        ImGui::End(); // end window
    }
}

static void render_disassembly_view(struct full_system &fsys, struct DVIEW &dview, bool update_dasm_scroll)
{
    struct disassembly_view *dasm = &dview.view->disassembly;
    struct cvec *dasm_rows = &dview.dasm_rows;
    if (fsys.enable_debugger) {
        char wname[100];
        snprintf(wname, sizeof(wname), "%s Disassembly View", dasm->processor_name.ptr);
        if (ImGui::Begin(wname)) {
            static ImGuiTableFlags flags =
                    ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                    ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable;


            static const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
            static const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();

            struct disassembly_vars dv = dasm->get_disassembly_vars.func(dasm->get_disassembly_vars.ptr,
                                                                              &fsys.dbgr, dasm);
            cvec_clear(dasm_rows);
            u32 cur_line_num = disassembly_view_get_rows(&fsys.dbgr, dasm, dv.address_of_executing_instruction,
                                                         20,
                                                         100, dasm_rows);
            u32 numcols = (dasm ? dasm->has_context ? 3 : 2 : 2);

            // When using ScrollX or ScrollY we need to specify a size for our table container!
            // Otherwise by default the table will fit all available space, like a BeginChild() call.
            ImVec2 outer_size = ImVec2(TEXT_BASE_WIDTH * 80, TEXT_BASE_HEIGHT * 20);
            if (ImGui::BeginTable("table_dasm", numcols, flags, outer_size)) {
                ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
                ImGui::TableSetupColumn("addr", ImGuiTableColumnFlags_None);
                ImGui::TableSetupColumn("disassembly", ImGuiTableColumnFlags_None);
                if (numcols == 3) ImGui::TableSetupColumn("context at last execution", ImGuiTableColumnFlags_None);
                ImGui::TableHeadersRow();
                ImGuiListClipper clipper;
                if (update_dasm_scroll) {
                    float scrl = clipper.ItemsHeight * (cur_line_num - 2);
                    float cur_scroll = ImGui::GetScrollY();
                    if ((cur_scroll > scrl) || (scrl < (cur_scroll + (clipper.ItemsHeight * 8))))
                        ImGui::SetScrollY(scrl);
                }
                clipper.Begin(100);
                while (clipper.Step()) {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                        ImGui::TableNextRow();
                        auto *strs = (struct disassembly_entry_strings *) cvec_get(dasm_rows, row);

                        ImGui::TableSetColumnIndex(0);
                        ImGui::Selectable(strs->addr, row == cur_line_num,
                                          ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_Disabled);
                        //ImGui::Text("%s", strs->addr);

                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%s", strs->dasm);
                        //ImGui::Selectable(strs->dasm, row == cur_line_num, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_Disabled);

                        if (numcols == 3) {
                            ImGui::TableSetColumnIndex(2);
                            ImGui::Text("%s", strs->context);
                        }
                    }
                }
                ImGui::EndTable();
                dasm->fill_view.func(dasm->fill_view.ptr, &fsys.dbgr, dasm);

                ImGui::SameLine();
                outer_size = ImVec2(TEXT_BASE_WIDTH * 30, TEXT_BASE_HEIGHT * 20);
                if (ImGui::BeginTable("table_registers", 2, flags, outer_size)) {
                    ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
                    ImGui::TableSetupColumn("register", ImGuiTableColumnFlags_None);
                    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_None);
                    ImGui::TableHeadersRow();

                    ImGuiListClipper clipper;
                    clipper.Begin((int) cvec_len(&dasm->cpu.regs));
                    while (clipper.Step()) {
                        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                            ImGui::TableNextRow();
                            auto *ctx = (struct cpu_reg_context *) cvec_get(&dasm->cpu.regs, row);

                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("%s", ctx->name);

                            ImGui::TableSetColumnIndex(1);
                            char rndr[50];
                            cpu_reg_context_render(ctx, rndr, sizeof(rndr));
                            ImGui::Text("%s", rndr);
                        }
                    }
                    ImGui::EndTable();

                }

            }
        }
        ImGui::End();
    }

}

static void render_disassembly_views(struct full_system &fsys, bool update_dasm_scroll) {
    for (auto &dv : fsys.dasm_views) {
        render_disassembly_view(fsys, dv, update_dasm_scroll);
    }
}

static void render_waveform_view(struct full_system &fsys, struct WVIEW &wview)
{
    fsys.waveform_view_present(wview);
    if (ImGui::Begin(wview.view->name)) {
        if (wview.waveforms[0].wf->default_clock_divider != 0) {
            if (wview.waveforms[0].wf->clock_divider == 0) wview.waveforms[0].wf->clock_divider = wview.waveforms[0].wf->default_clock_divider;
            static int a;
            static bool rv = false;
            a = wview.waveforms[0].wf->clock_divider;
            ImGui::DragInt("Clock divider", &a, 0.5f, 10, 500, "%d");
            /*
            ImGui::Checkbox("Randomly vary divider", &rv);
            static int b = 10;
            ImGui::DragInt("Vary by +/-", &b, 0.5f, 1, 100, "%d");
            if (rv) {
                int rn = ((int)(arc4random() % (b * 2))) - b;
                float perc = (float)rn / 100.0f;
                float vary = perc * (float)wview.waveforms[0].wf->default_clock_divider;
                wview.waveforms[0].wf->clock_divider = wview.waveforms[0].wf->default_clock_divider + vary;
            }
            else {*/
                wview.waveforms[0].wf->clock_divider = a;
            //}
        }
        u32 on_line = 0;
        u32 last_kind = 0;
        for (auto& wf : wview.waveforms) {
            u32 old_on_line = on_line;
            bool make_new_line = false;
            switch(wf.wf->kind) {
                case dwk_main:
                    on_line += 2;
                    break;
                case dwk_channel:
                    on_line++;
                    break;
                default:
                    assert(1==2);
            }
            switch(on_line) {
                case 1:
                    break;
                case 3:
                    make_new_line = true;
                    break;
                case 2:
                    //if (old_on_line == 0) break;
                    make_new_line = true;
                    break;
                default:
                    assert(1==2);
            }
            if (last_kind == dwk_main) {
                on_line = 0;
            }
            else if (!make_new_line) ImGui::SameLine();
            if (on_line >= 2) on_line = 0;
            last_kind = wf.wf->kind;
            ImGui::SetNextWindowSizeConstraints(ImVec2(wf.wf->samples_requested, wf.height), ImVec2(wf.wf->samples_requested+20, wf.height+30));
            if (ImGui::BeginChild(wf.wf->name, ImVec2(-FLT_MIN, 0.0f), ImGuiChildFlags_None)) {
                ImGui::Checkbox(wf.wf->name, &wf.output_enabled);
                wf.wf->ch_output_enabled = wf.output_enabled;
                ImGui::Image(wf.tex.for_image(), wf.tex.sz_for_display, wf.tex.uv0, wf.tex.uv1);
            }
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

static void render_radiogroup(struct debugger_widget *widget)
{
    if (widget->same_line) ImGui::SameLine();

    ImGuiWindowFlags window_flags = ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AutoResizeX;
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
    static int draw_lines = 2;
    static int max_height_in_lines = 2;

    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 1), ImVec2(FLT_MAX, ImGui::GetTextLineHeightWithSpacing() * max_height_in_lines));
    ImGui::BeginChild(widget->radiogroup.title, ImVec2(-FLT_MIN, 0.0f), window_flags);
    /*if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu(widget->radiogroup.title))
        {
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }*/
    int ch = int(widget->radiogroup.value);
    //         ImGui::RadioButton("radio a", &e, 0); ImGui::SameLine();
    //        ImGui::RadioButton("radio b", &e, 1); ImGui::SameLine();
    //        ImGui::RadioButton("radio c", &e, 2);
    for (u32 i = 0; i < cvec_len(&widget->radiogroup.buttons); i++) {
        struct debugger_widget *cb = (struct debugger_widget *)cvec_get(&widget->radiogroup.buttons, i);
        if (cb->same_line) ImGui::SameLine();
        ImGui::RadioButton(cb->checkbox.text, &ch, (int)cb->checkbox.value);
    }
    widget->radiogroup.value = u32(ch);

    ImGui::EndChild();
    ImGui::PopStyleVar();
}


static void render_checkbox(struct debugger_widget *widget)
{
    bool mval = widget->checkbox.value ? true : false;
    bool disabled = widget->enabled ? false : true;
    if (widget->same_line) ImGui::SameLine();
    ImGui::BeginDisabled(disabled);
    ImGui::Checkbox(widget->checkbox.text, &mval);
    ImGui::EndDisabled();
    widget->checkbox.value = mval ? 1 : 0;
}

static void render_debugger_widget(struct debugger_widget *widget)
{
    switch(widget->kind) {
        case JSMD_checkbox: {
            render_checkbox(widget);
            break; }
        case JSMD_radiogroup:
            render_radiogroup(widget);
            break;
        default:
            printf("\nWHAT KIND BAD %d", widget->kind);
            break;
    }
}


static void render_debugger_widgets(struct cvec *options)
{
    for (u32 i = 0; i < cvec_len(options); i++) {
        struct debugger_widget *widget = (struct debugger_widget *)cvec_get(options, i);
        render_debugger_widget(widget);
    }
}

static void render_image_views(struct full_system &fsys)
{
    for (auto &myv: fsys.images) {
        if (ImGui::Begin(myv.view->image.label)) {
            render_debugger_widgets(&myv.view->options);
            fsys.image_view_present(myv.view, myv.texture);
            ImGui::Image(myv.texture.for_image(), myv.texture.sz_for_display, myv.texture.uv0, myv.texture.uv1);
        }
        ImGui::End();
    }
}

static void render_opt_view(struct full_system &fsys)
{
    struct cvec *opts = &fsys.sys->opts;
    if (cvec_len(opts) > 0) {
        if (ImGui::Begin("Core Options")) {
            for (u32 i = 0; i < cvec_len(opts); i++) {
                struct debugger_widget *w = (struct debugger_widget *)cvec_get(opts, i);
                render_debugger_widget(w);
            }
        }
        ImGui::End();
    }
}

static void render_debug_views(struct full_system &fsys, ImGuiIO& io, bool update_dasm_scroll)
{
    render_event_view(fsys);
    render_disassembly_views(fsys, update_dasm_scroll);
    render_image_views(fsys);
    for (auto &wv : fsys.waveform_views) {
        render_waveform_view(fsys, wv);
    }
}

// Main code
int main(int, char**)
{
    struct inifile ini;
    load_inifile(&ini);
    char BIOS_BASE_PATH[500];
    char ROM_BASE_PATH[500];

    char *debugger_cols[3];
    constexpr size_t debugger_col_sizes[3] = { 10 * 1024, 100 * 1024, 500 * 1024 };
    debugger_cols[0] = (char *)malloc(debugger_col_sizes[0]);
    debugger_cols[1] = (char *)malloc(debugger_col_sizes[1]);
    debugger_cols[2] = (char *)malloc(debugger_col_sizes[2]);

    struct kv_pair *kvp = inifile_get_or_make_key(&ini, "general", "bios_base_path");
    assert(kvp);
    snprintf(BIOS_BASE_PATH, sizeof(BIOS_BASE_PATH), "%s", kvp->str_value);

    kvp = inifile_get_or_make_key(&ini, "general", "rom_base_path");
    assert(kvp);
    snprintf(BIOS_BASE_PATH, sizeof(ROM_BASE_PATH), "%s", kvp->str_value);

#ifdef DO_DREAMCAST
    enum jsm_systems which = SYS_DREAMCAST;
#else
    //enum jsm_systems which = SYS_ATARI2600;
    enum jsm_systems which = SYS_GENESIS_USA;
    //enum jsm_systems which = SYS_GBC;
    //enum jsm_systems which = SYS_APPLEIIe;
    //enum jsm_systems which = SYS_DMG;
    //enum jsm_systems which = SYS_NES;
    //enum jsm_systems which = SYS_SMS2;
    //enum jsm_systems which = SYS_GG;
    //enum jsm_systems which = SYS_SG1000;
    //enum jsm_systems which = SYS_MAC512K;
#endif

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return -1;
    }


#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    Uint32 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
    SDL_Window* window = SDL_CreateWindow("JSMooCh", 1280, 720, window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return -1;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(-1); // Enable adaptive vsync
    SDL_ShowWindow(window);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Emscripten allows preloading a file or folder to be accessible at runtime. See Makefile for details.
    //io.Fonts->AddFontDefault();
#ifndef IMGUI_DISABLE_FILE_FUNCTIONS
    //io.Fonts->AddFontFromFileTTF("fonts/segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("fonts/ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);
#endif

    // Our state
    static int done_break = 0;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    bool last_frame_was_whole = false;

    // Main loop
    static bool playing = true;
    bool done = false;
    full_system fsys;
    fsys.setup_system(which);
    if (!fsys.worked) {
        printf("\nCould not initialize system! %d", fsys.worked);
        return -1;
    }
    fsys.state = FSS_pause;
    fsys.has_played_once = false;
    fsys.setup_wgpu();
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!done)
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        //io.WantCaptureKeyboard = false;
#ifdef STOPAFTERAWHILE
        // 14742566
        framevars fv = fsys.get_framevars();
        if ((fv.master_frame > 240) && (!done_break) && (!dbg.do_break)) {
            //if (fv.master_cycle >= 12991854) {
            done_break = 1;
            dbg_break("Because", fv.master_cycle);
        }
#endif
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        // React to changes in screen size
        int width, height;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        framevars start_fv = fsys.get_framevars();

        fsys.enable_debugger = true;

        // Update the system
        if (dbg.do_break) {
            if (fsys.state == FSS_play)
                fsys.state = FSS_pause;
        }

        if (fsys.state == FSS_play) {
            update_input(&fsys, io);
            fsys.do_frame();
            last_frame_was_whole = true;
            fsys.events_view_present();
            fsys.has_played_once = true;
        }
        fsys.present();

        render_emu_window(fsys, io);

        // debug controls window
        if (ImGui::Begin("Play")){
            static int steps[3] = { 10, 1, 1 };
            bool play_pause = false;
            bool step_clocks = false, step_scanlines = false, step_seconds = false;
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
            if (ImGui::BeginChild("Playback Controls", ImVec2(200, 150), ImGuiChildFlags_Border, window_flags)) {
                play_pause = ImGui::Button("Play/pause");

                ImGui::PushID(1);
                step_clocks = ImGui::Button("Step");
                ImGui::PopID();
                ImGui::SameLine();
                ImGui::InputInt("cycles", &steps[0]);

                ImGui::PushID(2);
                step_scanlines = ImGui::Button("Step");
                ImGui::PopID();
                ImGui::SameLine();
                ImGui::InputInt("lines", &steps[1]);

                ImGui::PushID(3);
                step_seconds = ImGui::Button("Step");
                ImGui::PopID();
                ImGui::SameLine();
                ImGui::InputInt("frames", &steps[2]);


            }
            ImGui::EndChild(); // end sub-window
            ImGui::PopStyleVar();
            if (play_pause) {
                switch(fsys.state) {
                    case FSS_pause:
                        fsys.state = FSS_play;
                        dbg_unbreak();
                        fsys.audio.pause();
                        break;
                    case FSS_play:
                        fsys.audio.play();
                        fsys.state = FSS_pause;
                        break;
                }
            }
            if (step_scanlines) {
                dbg_unbreak();
                fsys.step_scanlines(steps[1]);
                last_frame_was_whole = false;
            }
            if (step_clocks) {
                dbg_unbreak();
                fsys.step_cycles(steps[0]);
                last_frame_was_whole = false;
            }
            if (step_seconds) {
                dbg_unbreak();
                fsys.step_seconds(steps[2]);
                last_frame_was_whole = false;
            }

            framevars fv = fsys.get_framevars();
            ImGui::SameLine();
            ImGui::Text("Frame: %lld", fv.master_frame);
            ImGui::Text("X,Y:%d,%d", fv.x, fv.scanline);
        }
        ImGui::End();

        framevars end_fv = fsys.get_framevars();
        bool update_dasm_scroll = false;
        if (start_fv.master_cycle != end_fv.master_cycle) {
            // Update stuff like scroll
            update_dasm_scroll = true;
        }

        // disassembly+ view
        render_opt_view(fsys);
        render_debug_views(fsys, io, update_dasm_scroll);


        // Rendering
        ImGui::Render();

        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        }

        SDL_GL_SwapWindow(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();


    fsys.destroy_system();

    inifile_delete(&ini);

    return 0;
}

