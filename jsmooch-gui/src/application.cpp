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
#include "../vendor/myimgui/backends/imgui_impl_glfw.h"
#include "../vendor/myimgui/backends/imgui_impl_wgpu.h"
#include "helpers/debug.h"
#include "keymap_translate.h"
#include "my_texture.h"
#include "full_sys.h"
#include "helpers/inifile.h"

//#define STOPAFTERAWHILE

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#else
#include <webgpu/webgpu_glfw.h>
#endif

#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>
#include <webgpu/webgpu_cpp.h>

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

// TODO:
// video doubling for <256 wide
// custom texture sizes
// set texture UVs
// encapsulate systems
// lots of UI work
// load directories from inifile

// Global WebGPU required states
static WGPUInstance      wgpu_instance = nullptr;
static WGPUDevice        wgpu_device = nullptr;
static WGPUSurface       wgpu_surface = nullptr;
static WGPUTextureFormat wgpu_preferred_fmt = WGPUTextureFormat_RGBA8Unorm;
static WGPUSwapChain     wgpu_swap_chain = nullptr;
static int               wgpu_swap_chain_width = 1280;
static int               wgpu_swap_chain_height = 720;

// Forward declarations
static bool InitWGPU(GLFWwindow* window);
static void CreateSwapChain(int width, int height);

static void glfw_error_callback(int error, const char* description)
{
    printf("GLFW Error %d: %s\n", error, description);
}

static void wgpu_error_callback(WGPUErrorType error_type, const char* message, void*)
{
    const char* error_type_lbl = "";
    switch (error_type)
    {
        case WGPUErrorType_Validation:  error_type_lbl = "Validation"; break;
        case WGPUErrorType_OutOfMemory: error_type_lbl = "Out of memory"; break;
        case WGPUErrorType_Unknown:     error_type_lbl = "Unknown"; break;
        case WGPUErrorType_DeviceLost:  error_type_lbl = "Device lost"; break;
        default:                        error_type_lbl = "Unknown";
    }
    printf("%s error: %s\n", error_type_lbl, message);
}

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
    //enum jsm_systems which = SYS_GENESIS;
    //enum jsm_systems which = SYS_GBC;
    enum jsm_systems which = SYS_DMG;
    //enum jsm_systems which = SYS_NES;
    //enum jsm_systems which = SYS_SMS2;
    //enum jsm_systems which = SYS_MAC512K;
#endif

    full_system fsys;
    fsys.setup_system(which);
    if (!fsys.worked) {
        printf("\nCould not initialize system! %d", fsys.worked);
        return -1;
    }
    fsys.state = FSS_pause;

#ifdef NEWSYS
    newsys(&fsys);
    return;
#endif


    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        return 1;
    }

    // Make sure GLFW does not initialize any graphics context.
    // This needs to be done explicitly later.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(wgpu_swap_chain_width, wgpu_swap_chain_height, "JSmooCh", nullptr, nullptr);
    if (window == nullptr) {
        return 1;
    }

    // Initialize the WebGPU environment
    if (!InitWGPU(window))
    {
        if (window)
            glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    CreateSwapChain(wgpu_swap_chain_width, wgpu_swap_chain_height);
    glfwSwapInterval(0);
    glfwShowWindow(window);
    static int last_frame_was_whole = true;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    bool has_played_once = false;
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOther(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
    ImGui_ImplWGPU_InitInfo init_info;
    init_info.Device = wgpu_device;
    init_info.NumFramesInFlight = 3;
    init_info.RenderTargetFormat = wgpu_preferred_fmt;
    init_info.DepthStencilFormat = WGPUTextureFormat_Undefined;
    ImGui_ImplWGPU_Init(&init_info);

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

    /*
    std::vector<uint8_t> pixels(4 * 512 * 512);
    for (uint32_t i = 0; i < 512; ++i) {
        for (uint32_t j = 0; j < 512; ++j) {
            uint8_t *p = &pixels[4 * (j * 512 + i)];
            p[0] = (uint8_t)(i >> 1); // r
            p[1] = (uint8_t)(j >> 1); // g
            p[2] = 128; // b
            p[3] = 255; // a
        }
    }
    //backbuffer.upload_data(pixels.data(), pixels.size(), 512, 512);*/


    // Our state
    bool show_demo_window = false;
    bool show_another_window = true;
    static int done_break = 0;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    fsys.wgpu_device = wgpu_device;

    // Main loop
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    fsys.setup_wgpu();

    static bool playing = true;
    while (!glfwWindowShouldClose(window))
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
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        // React to changes in screen size
        int width, height;
        glfwGetFramebufferSize((GLFWwindow*)window, &width, &height);
        if (width != wgpu_swap_chain_width || height != wgpu_swap_chain_height)
        {
            ImGui_ImplWGPU_InvalidateDeviceObjects();
            CreateSwapChain(width, height);
            ImGui_ImplWGPU_CreateDeviceObjects();
        }
        // Start the Dear ImGui frame
        ImGui_ImplWGPU_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        framevars start_fv = fsys.get_framevars();

        static bool enable_debugger = true;

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
            has_played_once = true;
        }
        fsys.present();

        // 1. Show playback controls
        {
            static int steps[3] = { 10, 1, 1 };
            bool play_pause = false;
            bool step_clocks = false, step_scanlines = false, step_seconds = false;
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
            if (ImGui::BeginChild("Event Viewer Events", ImVec2(200, 150), ImGuiChildFlags_Border, window_flags)) {
                play_pause = ImGui::Button("Play/pause");

                step_clocks = ImGui::Button("Step");
                ImGui::SameLine();
                ImGui::InputInt("cycles", &steps[0]);

                step_scanlines = ImGui::Button("Step");
                ImGui::SameLine();
                ImGui::InputInt("lines", &steps[1]);

                step_seconds = ImGui::Button("Step");
                ImGui::SameLine();
                ImGui::InputInt("frames", &steps[2]);


                ImGui::EndChild(); // end sub-window
            }
            ImGui::PopStyleVar();
            if (play_pause) {
                switch(fsys.state) {
                    case FSS_pause:
                        fsys.state = FSS_play;
                        dbg_unbreak();
                        break;
                    case FSS_play:
                        fsys.state = FSS_pause;
                        break;
                }
            }
            if (step_scanlines) {
                fsys.step_scanlines(steps[1]);
                last_frame_was_whole = false;
            }
            if (step_clocks) {
                fsys.step_cycles(steps[0]);
                last_frame_was_whole = false;
            }
            if (step_seconds) {
                fsys.step_seconds(steps[2]);
                last_frame_was_whole = false;
            }

            framevars fv = fsys.get_framevars();
            ImGui::SameLine();
            ImGui::Text("Frame: %lld", fv.master_frame);
            ImGui::Text("X,Y:%d,%d", fv.x, fv.scanline);
        }

        // Main emu window
        if (ImGui::Begin(fsys.sys->label)) {
            ImGui::Checkbox("2x Zoom", &fsys.output.zoom);
            ImGui::SameLine();
            ImGui::Checkbox("Hide Overscan", &fsys.output.hide_overscan);



            //ImGui::Text("This is some useful text.");                     // Display some text (you can use a format strings too)
            ImGui::Image(fsys.output.backbuffer_texture.for_image(), fsys.output_size(), fsys.output_uv0(), fsys.output_uv1());

            /*ImGui::SliderFloat("float", &f, 0.0f, 1.0f);                  // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color);       // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                                  // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);
            */
            ImGui::Text("FPS: %.1f", io.Framerate);
            ImGui::End();
        }

        framevars end_fv = fsys.get_framevars();
        bool update_dasm_scroll = false;
        if (start_fv.master_cycle != end_fv.master_cycle) {
            // Update stuff like scroll
            update_dasm_scroll = true;
        }

        // disassembly view
        if (fsys.dasm && enable_debugger && has_played_once && ImGui::Begin("Disassembly View")) {
            static ImGuiTableFlags flags =
                    ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                    ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable;

            //PushStyleCompact();
            //ImGui::CheckboxFlags("ImGuiTableFlags_ScrollY", &flags, ImGuiTableFlags_ScrollY);
            //PopStyleCompact();
            static const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
            static const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();

            struct disassembly_vars dv = fsys.dasm->get_disassembly_vars.func(fsys.dasm->get_disassembly_vars.ptr,
                                                                              &fsys.dbgr, fsys.dasm);
            // void (*func)(void *, struct debugger_interface *, struct disassembly_view *, u32 start_addr, u32 lines);
            u32 cur_line_num = disassembly_view_get_rows(&fsys.dbgr, fsys.dasm, dv.address_of_executing_instruction, 20,
                                                         40, &fsys.dasm_rows);
            u32 numcols = (fsys.dasm ? fsys.dasm->has_context ? 3 : 2 : 2);

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
                        auto *strs = (struct disassembly_entry_strings *) cvec_get(&fsys.dasm_rows, row);

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
                fsys.dasm->fill_view.func(fsys.dasm->fill_view.ptr, &fsys.dbgr, fsys.dasm);

                ImGui::SameLine();
                outer_size = ImVec2(TEXT_BASE_WIDTH * 30, TEXT_BASE_HEIGHT * 20);
                if (ImGui::BeginTable("table_registers", 2, flags, outer_size)) {
                    ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
                    ImGui::TableSetupColumn("register", ImGuiTableColumnFlags_None);
                    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_None);
                    ImGui::TableHeadersRow();

                    ImGuiListClipper clipper;
                    clipper.Begin((int)cvec_len(&fsys.dasm->cpu.regs));
                    while (clipper.Step()) {
                        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                            ImGui::TableNextRow();
                            auto *ctx = (struct cpu_reg_context *)cvec_get(&fsys.dasm->cpu.regs, row);

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
            ImGui::End();
        }

        for (auto &myv: fsys.images) {
            if (ImGui::Begin(myv.view->image.label)) {
                fsys.image_view_present(myv.view, myv.texture);
                ImGui::Image(myv.texture.for_image(), myv.texture.sz_for_display, myv.texture.uv0, myv.texture.uv1);
                ImGui::End();
            }
        }

        if (fsys.events.view && ImGui::Begin("Event Viewer")) {
            static bool ozoom = true;
            ImGui::Checkbox("2x Zoom", &ozoom);
            fsys.events_view_present();
            ImGui::Image(fsys.events.texture.for_image(), fsys.events.texture.zoom_sz_for_display(ozoom ? 2  : 1), fsys.events.texture.uv0, fsys.events.texture.uv1, {1.0, 1.0, 1.0, 1.0}, {1.0, 1.0, 1.0, 1.0});
            static bool things_open[50];
            static float color_edits[50*10][3];
            u32 idx = 0;
            for (u32 cati = 0; cati < cvec_len(&fsys.events.view->categories); cati++) {
                auto *cat = (struct event_category *)cvec_get(&fsys.events.view->categories, cati);
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
                if (ImGui::TreeNodeEx(cat->name, flags)) {
                    for (u32 evi = 0; evi < cvec_len(&fsys.events.view->events); evi++) {
                        auto *event = (struct debugger_event *)cvec_get(&fsys.events.view->events, evi);
                        if (event->category_id == cat->id) {
                            color_edits[idx][0] = (float)(event->color & 0xFF) / 255.0;
                            color_edits[idx][1] = (float)((event->color >> 8) & 0xFF) / 255.0;
                            color_edits[idx][2] = (float)((event->color >> 16) & 0xFF) / 255.0;
                            ImGui::ColorEdit3(event->name, color_edits[idx], ImGuiColorEditFlags_NoInputs);
                            idx++;
                        }
                    }
                    ImGui::TreePop();
                }
            }

            //ImGui::EndChild(); // end sub-window
            //ImGui::PopStyleVar();

            ImGui::End(); // end window
        }

        // Rendering
        ImGui::Render();

#ifndef __EMSCRIPTEN__
        // Tick needs to be called in Dawn to display validation errors
        wgpuDeviceTick(wgpu_device);
#endif

        WGPURenderPassColorAttachment color_attachments = {};
        color_attachments.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        color_attachments.loadOp = WGPULoadOp_Clear;
        color_attachments.storeOp = WGPUStoreOp_Store;
        color_attachments.clearValue = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        color_attachments.view = wgpuSwapChainGetCurrentTextureView(wgpu_swap_chain);

        WGPURenderPassDescriptor render_pass_desc = {};
        render_pass_desc.colorAttachmentCount = 1;
        render_pass_desc.colorAttachments = &color_attachments;
        render_pass_desc.depthStencilAttachment = nullptr;

        WGPUCommandEncoderDescriptor enc_desc = {};
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(wgpu_device, &enc_desc);

        WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_desc);
        ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);
        wgpuRenderPassEncoderEnd(pass);

        WGPUCommandBufferDescriptor cmd_buffer_desc = {};
        WGPUCommandBuffer cmd_buffer = wgpuCommandEncoderFinish(encoder, &cmd_buffer_desc);
        WGPUQueue queue = wgpuDeviceGetQueue(wgpu_device);

        // MYXX

        // NOTMYXX

        wgpuQueueSubmit(queue, 1, &cmd_buffer);


#ifndef __EMSCRIPTEN__
        wgpuSwapChainPresent(wgpu_swap_chain);
#endif

        wgpuTextureViewRelease(color_attachments.view);
        wgpuRenderPassEncoderRelease(pass);
        wgpuCommandEncoderRelease(encoder);
        wgpuCommandBufferRelease(cmd_buffer);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif


    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    fsys.destroy_system();

    inifile_delete(&ini);

    return 0;
}

#ifndef __EMSCRIPTEN__
static WGPUAdapter RequestAdapter(WGPUInstance instance)
{
    auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* pUserData)
    {
        if (status == WGPURequestAdapterStatus_Success)
            *(WGPUAdapter*)(pUserData) = adapter;
        else
            printf("Could not get WebGPU adapter: %s\n", message);
    };
    WGPUAdapter adapter;
    wgpuInstanceRequestAdapter(instance, nullptr, onAdapterRequestEnded, (void*)&adapter);
    return adapter;
}

static WGPUDevice RequestDevice(WGPUAdapter& adapter)
{
    auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, const char* message, void* pUserData)
    {
        if (status == WGPURequestDeviceStatus_Success)
            *(WGPUDevice*)(pUserData) = device;
        else
            printf("Could not get WebGPU device: %s\n", message);
    };
    WGPUDevice device;
    wgpuAdapterRequestDevice(adapter, nullptr, onDeviceRequestEnded, (void*)&device);
    return device;
}
#endif

static bool InitWGPU(GLFWwindow* window)
{
    wgpu::Instance instance = wgpuCreateInstance(nullptr);

#ifdef __EMSCRIPTEN__
    wgpu_device = emscripten_webgpu_get_device();
    if (!wgpu_device)
        return false;
#else
    WGPUAdapter adapter = RequestAdapter(instance.Get());
    if (!adapter)
        return false;
    wgpu_device = RequestDevice(adapter);
#endif

#ifdef __EMSCRIPTEN__
    wgpu::SurfaceDescriptorFromCanvasHTMLSelector html_surface_desc = {};
    html_surface_desc.selector = "#canvas";
    wgpu::SurfaceDescriptor surface_desc = {};
    surface_desc.nextInChain = &html_surface_desc;
    wgpu::Surface surface = instance.CreateSurface(&surface_desc);

    wgpu::Adapter adapter = {};
    wgpu_preferred_fmt = (WGPUTextureFormat)surface.GetPreferredFormat(adapter);
#else
    wgpu::Surface surface = wgpu::glfw::CreateSurfaceForWindow(instance, window);
    if (!surface)
        return false;
    wgpu_preferred_fmt = WGPUTextureFormat_BGRA8Unorm;
#endif

    wgpu_instance = instance.MoveToCHandle();
    wgpu_surface = surface.MoveToCHandle();

    wgpuDeviceSetUncapturedErrorCallback(wgpu_device, wgpu_error_callback, nullptr);

    return true;
}

static void CreateSwapChain(int width, int height)
{
    if (wgpu_swap_chain)
        wgpuSwapChainRelease(wgpu_swap_chain);
    wgpu_swap_chain_width = width;
    wgpu_swap_chain_height = height;
    WGPUSwapChainDescriptor swap_chain_desc = {};
    swap_chain_desc.usage = WGPUTextureUsage_RenderAttachment;
    swap_chain_desc.format = wgpu_preferred_fmt;
    swap_chain_desc.width = width;
    swap_chain_desc.height = height;
    swap_chain_desc.presentMode = WGPUPresentMode_Fifo;
    wgpu_swap_chain = wgpuDeviceCreateSwapChain(wgpu_device, wgpu_surface, &swap_chain_desc);
}