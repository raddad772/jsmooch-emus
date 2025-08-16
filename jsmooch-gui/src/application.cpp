// Dear ImGui: standalone example application for using GLFW + WebGPU
// - Emscripten is supported for publishing on web. See https://emscripten.org.
// - Dawn is used as a WebGPU implementation on desktop.

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp
#include <cstdio>
#include "build.h"
#include "application.h"

#include "imgui_internal.h"
#include <SDL3/SDL.h>
#define FRAME_MULTI 10

#ifdef JSM_OPENGL
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL3/SDL_opengles2.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif
#endif // jsm_opengl

#ifdef JSM_WEBGPU
#include <cstdio>

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

// Global WebGPU required states
static WGPUSwapChain     wgpu_swap_chain = nullptr;
static WGPUInstance      wgpu_instance = nullptr;
static WGPUDevice        wgpu_device = nullptr;
static WGPUSurface       wgpu_surface = nullptr;
static WGPUTextureFormat wgpu_preferred_fmt = WGPUTextureFormat_RGBA8Unorm;
static int               wgpu_swap_chain_width = 1280;
static int               wgpu_swap_chain_height = 720;

// Forward declarations
static bool InitWGPU(GLFWwindow* window);
static void CreateSwapChain(int width, int height);

#endif


#include "helpers/debug.h"
#include "keymap_translate.h"
#include "my_texture.h"
#include "full_sys.h"
#include "helpers/inifile.h"
#include "helpers/serialize/serialize.h"

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

static void update_input(struct full_system* fsys, u32 *hotkeys, ImGuiIO& io) {
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
        hotkeys[0] = ImGui::IsKeyPressed(ImGuiKey_K, false); // save
        hotkeys[1] = ImGui::IsKeyPressed(ImGuiKey_L, false); // load
        // Handle controller 1
        if (fsys->io.controller1.vec) {
            auto *pio = (struct physical_io_device *)cpg(fsys->io.controller1);
            if (pio->connected && pio->enabled) {
                struct JSM_CONTROLLER *ctr = &pio->controller;
                for (u32 i = 0; i < cvec_len(&ctr->digital_buttons); i++) {
                    struct HID_digital_button *db = (struct HID_digital_button *)cvec_get(&ctr->digital_buttons, i);
                    db->state = 0;
                    db->state |= ImGui::IsKeyDown(jk_to_imgui(db->common_id));
                    db->state |= ImGui::IsKeyDown(jk_to_imgui_gp(db->common_id));
                }
            }
        }
    //}
}

struct mouse_emu_struct {
    u32 is_in_frame;
    i32 x, y;
    u32 button_down;
};

static void get_mouse_coords(ImGuiIO& io, struct mouse_emu_struct &a)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    a.x = (i32)(io.MousePos.x - window->Pos.x) - 8;
    a.y = (i32)(io.MousePos.y - window->Pos.y) - 52;
    a.button_down = ImGui::IsMouseDown(0);
}

static void render_emu_window(struct full_system &fsys, ImGuiIO& io, u32 frame_multi)
{
    //ImVec2 window_origin = ImGui::GetCursorPos();
    if (ImGui::Begin(fsys.sys->label)) {
        ImGui::Checkbox("2x Zoom", &fsys.output.zoom);
        ImGui::SameLine();
        ImGui::Checkbox("Hide Overscan", &fsys.output.hide_overscan);
        //ImGui::InvisibleButton("hidey screen!", fsys.output_size());
        //ImGui::Image(fsys.output.backbuffer_texture.for_image(), fsys.output_size(), fsys.output_uv0(), fsys.output_uv1());
        //ImVec2 origin_pos = ImGui::GetCursorPos();
        /*origin_pos.x -= window_origin.x;
        origin_pos.y -= window_origin.y;*/
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0,0});
        ImGui::ImageButton("RENDER GAME", fsys.output.backbuffer_texture.for_image(), fsys.output_size(), fsys.output_uv0(), fsys.output_uv1());
        ImGui::PopStyleVar();

        struct mouse_emu_struct a;
        get_mouse_coords(io, a);
        fsys.update_touch(a.x, a.y, a.button_down);
        ImGui::Text("FPS: %.1f", io.Framerate * frame_multi);
        ImGui::Text("CORDS: %d %d", a.x, a.y);

    }
    ImGui::End();
}

#define MEMORY_VIEW_DEFAULT_ENABLE 0
#define EVENT_VIEWER_DEFAULT_ENABLE 0
#define DISASM_VIEW_DEFAULT_ENABLE 0
#define IMAGE_VIEW_DEFAULT_ENABLE 0
#define DBGLOG_VIEW_DEFAULT_ENABLE 0
#define SOUND_VIEW_DEFAULT_ENABLE 0
#define TRACE_VIEW_DEFAULT_ENABLE 0
#define CONSOLE_VIEW_DEFAULT_ENABLE 0

int hexfilter(ImGuiInputTextCallbackData *data)
{
    u32 c = data->EventChar;
    if (!(((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'f')) || ((c >= 'A') && (c <= 'F')))) return 1;

    return 0;
}

void imgui_jsmooch_app::render_memory_view() {
    if (fsys.memory.view && fsys.debugger_setup) {
        struct managed_window *mw = register_managed_window(0x30000, mwk_debug_memory, "Memory Viewer",
                                                            MEMORY_VIEW_DEFAULT_ENABLE);
        struct memory_view *mv = fsys.memory.view;
        u32 num_modules = memory_view_num_modules(mv);
        if (mw->enabled && num_modules > 0) {
            if (ImGui::Begin("Memory Viewer")) {
                // Dropdown, numeric input, table
                static ImGuiComboFlags flags = 0;
                char *module_names[50];
                for (u32 i = 0; i < num_modules; i++) {
                    struct memory_view_module *mm = memory_view_get_module(mv, i);
                    assert(mm);
                    module_names[i] = mm->name;
                }
                int item_selected = mv->current_id;
                if (ImGui::BeginCombo("Memory Select", module_names[mv->current_id], flags)) {
                    for (int n = 0; n < num_modules; n++) {
                        const bool is_selected = (item_selected == n);
                        if (ImGui::Selectable(module_names[n], is_selected))
                            item_selected = n;

                        // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                u32 old_selected = mv->current_id;
                mv->current_id = item_selected;
                struct memory_view_module *mm = memory_view_get_module(mv, mv->current_id);
                if (old_selected != item_selected) {
                    mv->display_start_addr &= (1 << (4 * mm->addr_digits)) - 1;
                }

                char format_string[20];
                snprintf(format_string, 20, "%%0%dx", mm->addr_digits);

                // Now get input
                //ImGui::SameLine();
                static char addr_str[18];
                snprintf(addr_str, 18, format_string, mv->display_start_addr);
                mm->input_buf = addr_str;
                u32 old_entered_address = mv->display_start_addr;

                ImGui::InputText("Addr", addr_str, 18, ImGuiInputTextFlags_CallbackCharFilter, &hexfilter, mm);
                addr_str[mm->addr_digits] = 0;
                u32 newly_entered_address = strtol(addr_str, NULL, 16) & 0xFFFFFFF0;
                mv->display_start_addr = newly_entered_address;
                u32 entered_address_changed = mv->display_start_addr != old_entered_address;

                u32 old_top_displayed_line = mv->display_start_addr << 4;
                u32 top_displayed_line = 0xFFFFFFFF;

                // OK start clipper
                static ImGuiTableFlags itflags =
                        ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                        ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp;

                if (ImGui::BeginTable("mem_view_table", 3, itflags)) {
                    ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
                    ImGui::TableSetupColumn("Addr", ImGuiTableColumnFlags_None, mm->addr_digits);
                    ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_None, 32 + 16);
                    ImGui::TableSetupColumn("ASCII", ImGuiTableColumnFlags_None, 17);
                    ImGui::TableHeadersRow();
                    ImGuiListClipper clipper;
                    u32 num_lines = ((mm->addr_end - mm->addr_start) + 1) >> 4;
                    clipper.Begin(num_lines);
                    u8 data_buf[16];
                    char hex_buf[50];
                    char ascii_buf[18];
                    u32 old_end = 0;
                    u32 j = 0;
                    while (clipper.Step()) {
                        if (j == 1) {
                            if ((old_end == 1) && (clipper.DisplayStart == 1)) top_displayed_line = 0;
                            else if (clipper.DisplayStart < top_displayed_line) top_displayed_line = clipper.DisplayStart;
                        }

                        old_end = clipper.DisplayEnd;
                        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                            u32 addr = (row << 4) - mm->addr_start;
                            memory_view_get_line(mm, addr, (char *)data_buf);
                            hex_buf[0] = 0;
                            ascii_buf[0] = 0;
                            char *hp = hex_buf;
                            char *ap = ascii_buf;
                            for (u32 i = 0; i < 16; i++) {
                                u32 db = data_buf[i];
                                assert(db < 256);
                                snprintf(hp, 50 - (i * 3), "%02X ", db);
                                hp += 3;

                                if ((db >= 32) && (db <= 126)) {

                                }
                                else {
                                    db = '.';
                                }
                                snprintf(ap, 18 - i, "%c", db);
                                ap++;
                            }
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text(format_string, row << 4);

                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text(" %s", hex_buf);

                            ImGui::TableSetColumnIndex(2);
                            ImGui::Text(" %s", ascii_buf);
                        }
                        j++;
                    }
                    top_displayed_line <<= 4;
                    static u32 old_displayed_line_gui = 0;
                    if (old_top_displayed_line != top_displayed_line) {
                        // Address changed here...
                        mv->display_start_addr = top_displayed_line;
                    }
                    old_displayed_line_gui = top_displayed_line;
                    if (entered_address_changed) {
                        mv->display_start_addr = newly_entered_address;
                    }
                    float scrl;
                    scrl = clipper.ItemsHeight * (float)(mv->display_start_addr >> 4);
                    float cur_scroll = ImGui::GetScrollY();
                    if (cur_scroll != scrl) {
                        ImGui::SetScrollY(scrl);
                    }

                    ImGui::EndTable();
                }
            }
            ImGui::End(); // end window
        }
    }

}

void imgui_jsmooch_app::render_event_view()
{
    struct managed_window *mw = register_managed_window(100, mwk_debug_events, "Event Viewer", EVENT_VIEWER_DEFAULT_ENABLE);
    if (mw->enabled && fsys.events.view && fsys.has_played_once) {
        if (ImGui::Begin("Event Viewer")) {
            static bool ozoom = false;
            ImGui::Checkbox("2x Zoom", &ozoom);
            //fsys.pre_events_view_present();
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
                            bool mval = event->display_enabled;
                            ImGui::PushID(evi);
                            ImGui::Checkbox("", &mval);
                            event->display_enabled = mval;
                            ImGui::PopID();
                            ImGui::SameLine();
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

static bool rn_checkboxes[MAX_DBGLOG_IDS*4];

static void render_node(struct dbglog_view *view, struct dbglog_category_node *node, u32 *id_ptr) {
    // If we're a leaf...
    u32 id = (*id_ptr)++;

    if (cvec_len(&node->children) == 0) {
        // We are a leaf
        rn_checkboxes[id] = node->enabled;
        ImGui::Checkbox(node->name, &rn_checkboxes[id]);
        node->enabled = rn_checkboxes[id];
        view->ids_enabled[node->category_id] = rn_checkboxes[id];
    } else {
        // We are a further branch
        ImGui::SetNextItemOpen(true);
        ImGui::PushID(id);
        if (ImGui::TreeNodeEx(node->name)) {
            for (u32 i = 0; i < cvec_len(&node->children); i++) {
                struct dbglog_category_node *e = (struct dbglog_category_node *) cvec_get(&node->children, i);
                render_node(view, e, id_ptr);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

static ImVec4 get_iv4(u32 color)
{
    return {float((color >> 16) & 0xFF) / 255.0f,
                  float((color >> 8) & 0xFF) / 255.0f,
                  float((color) & 0xFF) / 255.0f,
                  1.0f};
}

void imgui_jsmooch_app::render_dbglog_view(struct DLVIEW &dview, bool update_dasm_scroll) {
    // 2 views
    char wname[100];
    struct debugger_view *mydv = dview.view;
    struct dbglog_view *dlv = &mydv->dbglog;
    snprintf(wname, sizeof(wname), "%s (visibility tree)", dlv->name);
    if (ImGui::Begin(wname)) {
        // Now do a tree of checkboxes!!!
        u32 bid = 0;
        struct dbglog_category_node *root = dbglog_category_get_root(dlv);
        for (u32 i = 0; i < cvec_len(&root->children); i++) {
            struct dbglog_category_node *c = (struct dbglog_category_node *) cvec_get(&root->children, i);
            render_node(dlv, c, &bid);
        }
    }
    ImGui::End();

    snprintf(wname, sizeof(wname), "%s", dlv->name);
    static char text[8 * 1024 * 1024];
    static int first = 1;
    if (first) {
        first = 0;
        memset(text, 0, 10);
    }
    if (ImGui::Begin(wname)) {
        static ImGuiTableFlags flags =
                ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp;
        if (ImGui::BeginTable("tabley_table", dlv->has_extra ? 4 : 3, flags)) {
            ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
            ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_None, 1);
            ImGui::TableSetupColumn("Timecode", ImGuiTableColumnFlags_None, 1);
            ImGui::TableSetupColumn("Entry", ImGuiTableColumnFlags_None, 5);
            if (dlv->has_extra)
                ImGui::TableSetupColumn("Extra", ImGuiTableColumnFlags_None, 5);

            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            u32 num_items = dbglog_count_visible_lines(dlv);

            /*if (dlv->updated) {
                dlv->updated = 0;
                float scrl = clipper.ItemsHeight * num_items;
                float cur_scroll = ImGui::GetScrollY();
                printf("\nUPDATED! cur_scroll:%f scrl:%f IH:%f", cur_scroll, scrl, clipper.ItemsHeight * 8);
                //if ((cur_scroll > scrl) || (scrl < (cur_scroll + (clipper.ItemsHeight * 8))))
                    ImGui::SetScrollY(10);
            }*/

            clipper.Begin(num_items);
            while (clipper.Step()) {
                u32 idx = dbglog_get_nth_visible(dlv, clipper.DisplayStart);
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    struct dbglog_entry *e = &dlv->items.data[idx];

                    ImGui::TextColored(get_iv4(dlv->id_to_color[e->category_id]), "%s", dlv->id_to_category[e->category_id]->short_name);

                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%lld", e->timecode);

                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextColored(get_iv4(dlv->id_to_color[e->category_id]), "%s", e->text.ptr);

                    if (dlv->has_extra) {
                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextColored(get_iv4(dlv->id_to_color[e->category_id]), "%s", e->extra.ptr);
                    }
                    idx = dbglog_get_next_visible(dlv, idx);
                }
                if (dlv->updated) {
                    dlv->updated--;
                    ImGuiContext& g = *GImGui;
                    ImGuiWindow* window = g.CurrentWindow;
                    ImGui::SetScrollY(window, window->ScrollMax.y);
                }
            }
            ImGui::EndTable();

        }
    }
    ImGui::End();
}


void imgui_jsmooch_app::render_disassembly_view(struct DVIEW &dview, bool update_dasm_scroll, u32 num)
{
    struct disassembly_view *dasm = &dview.view->disassembly;
    struct cvec *dasm_rows = &dview.dasm_rows;
    char wname[100];
    snprintf(wname, sizeof(wname), "%s Disassembly View", dasm->processor_name.ptr);
    struct managed_window *mw = register_managed_window(0x400 + num, mwk_debug_disassembly, wname, DISASM_VIEW_DEFAULT_ENABLE);
    if (mw->enabled && fsys.enable_debugger) {
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
                        if (!strs) continue;

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

void imgui_jsmooch_app::render_disassembly_views(bool update_dasm_scroll) {
    u32 i = 0;
    for (auto &dv : fsys.dasm_views) {
        render_disassembly_view(dv, update_dasm_scroll, i++);
    }
}

void imgui_jsmooch_app::render_waveform_view(struct WVIEW &wview, u32 num)
{
    struct managed_window *mw = register_managed_window(0x600 + num, mwk_debug_sound, wview.view->name, SOUND_VIEW_DEFAULT_ENABLE);
    if (mw->enabled) {
        fsys.waveform_view_present(wview);
        if (ImGui::Begin(wview.view->name)) {
            if (wview.waveforms[0].wf->default_clock_divider != 0) {
                if (wview.waveforms[0].wf->clock_divider == 0)
                    wview.waveforms[0].wf->clock_divider = wview.waveforms[0].wf->default_clock_divider;
                static int a;
                static bool rv = false;
                a = wview.waveforms[0].wf->clock_divider;
                ImGui::DragInt("Clock divider", &a, 0.5f, 10, 1500, "%d");
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
            for (auto &wf: wview.waveforms) {
                u32 old_on_line = on_line;
                bool make_new_line = false;
                switch (wf.wf->kind) {
                    case dwk_main:
                        on_line += 2;
                        break;
                    case dwk_channel:
                        on_line++;
                        break;
                    default:
                        assert(1 == 2);
                }
                switch (on_line) {
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
                        assert(1 == 2);
                }
                if (last_kind == dwk_main) {
                    on_line = 0;
                } else if (!make_new_line) ImGui::SameLine();
                if (on_line >= 2) on_line = 0;
                last_kind = wf.wf->kind;
                ImGui::SetNextWindowSizeConstraints(ImVec2(wf.wf->samples_requested, wf.height),
                                                    ImVec2(wf.wf->samples_requested + 20, wf.height + 30));
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
}

static void render_radiogroup(struct debugger_widget *widget)
{
    if (widget->same_line) ImGui::SameLine();

    ImGuiWindowFlags window_flags = ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AutoResizeX;
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
    static int draw_lines = 2;
    static int max_height_in_lines = 4;

    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 2), ImVec2(270, ImGui::GetTextLineHeightWithSpacing() * max_height_in_lines));
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

static void render_textbox(struct debugger_widget *widget)
{
    if (widget->same_line) ImGui::SameLine();
    ImGui::Text("%s", widget->textbox.contents.ptr);
}

static void render_colorkey(struct debugger_widget *widget)
{
    struct debugger_widget_colorkey *ck = &widget->colorkey;
    ImGui::Text("%s", ck->title);
    for (u32 i = 0; i < ck->num_items; i++) {
        struct debugger_widget_colorkey_item *item = &ck->items[i];
        ImGui::PushID(i);
        ImGui::BeginDisabled();
        u32 r = item->color & 0xFF;
        u32 g = (item->color >> 8) & 0xFF;
        u32 b = (item->color >> 16) & 0xFF;
        u32 cp = item->color | 0xFF000000;
        ImGui::PushStyleColor(ImGuiCol_Button, cp);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, cp);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, cp);
        ImGui::Button(" ");
        ImGui::EndDisabled();
        ImGui::PopStyleColor(3);
        ImGui::PopID();
        ImGui::SameLine();
        ImGui::Text("%s", item->name);
    }
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


static void render_debugger_post_widget(struct debugger_widget *widget)
{
    switch(widget->kind) {
        case JSMD_colorkey:
            render_colorkey(widget);
            break;
        case JSMD_radiogroup:
        case JSMD_checkbox:
        case JSMD_textbox:
            break;
        default:
            printf("\nWHAN O POST BA %d", widget->kind);
    }
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
        case JSMD_textbox:
            render_textbox(widget);
            break;
        case JSMD_colorkey:
            break;
        default:
            printf("\nWHAT KIND BAD %d", widget->kind);
            break;
    }
}

static void render_debugger_post_widgets(struct cvec *options)
{
    for (u32 i = 0; i < cvec_len(options); i++) {
        struct debugger_widget *widget = (struct debugger_widget *) cvec_get(options, i);
        render_debugger_post_widget(widget);
    }
};

static void render_debugger_widgets(struct cvec *options)
{
    for (u32 i = 0; i < cvec_len(options); i++) {
        struct debugger_widget *widget = (struct debugger_widget *)cvec_get(options, i);
        render_debugger_widget(widget);
    }
}

void imgui_jsmooch_app::render_console_view(bool update_dasm_scroll)
{
    u32 wi;
    static char text[3 * 1024 * 1024];
    for (auto &myv: fsys.console_views) {
        struct managed_window *mw = register_managed_window(0xC00 + (wi++), mwk_debug_console, myv.view->console.name, CONSOLE_VIEW_DEFAULT_ENABLE);
        struct console_view *tv = &myv.view->console;
        if (mw->enabled) {
            if (ImGui::Begin(myv.view->console.name)) {
                console_view_render_to_buffer(tv, text, sizeof(text));
                static ImGuiInputTextFlags flags = ImGuiInputTextFlags_ReadOnly;
                ImGui::InputTextMultiline("##source", text, IM_ARRAYSIZE(text), ImVec2(-FLT_MIN, -FLT_MIN), flags);

                if (tv->updated) {
                    tv->updated = 0;
                    ImGuiContext& g = *GImGui;
                    const char* child_window_name = NULL;
                    ImFormatStringToTempBuffer(&child_window_name, NULL, "%s/%s_%08X", g.CurrentWindow->Name, "##source", ImGui::GetID("##source"));
                    ImGuiWindow* child_window = ImGui::FindWindowByName(child_window_name);
                    ImGui::SetScrollY(child_window, child_window->ScrollMax.y);
                }
            }
            ImGui::End();
        }
    }
}

void imgui_jsmooch_app::render_trace_view(bool update_dasm_scroll)
{
    u32 wi = 0;
    static const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
    static const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();
    for (auto &myv: fsys.trace_views) {
        struct managed_window *mw = register_managed_window(0x800 + (wi++), mwk_debug_trace, myv.view->trace.name, TRACE_VIEW_DEFAULT_ENABLE);
        struct trace_view *tv = &myv.view->trace;
        if (mw->enabled) {
            if (ImGui::Begin(myv.view->trace.name)) {
                static ImGuiTableFlags flags =
                        ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                        ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp;
                u32 total_sz = 0;
                float widths[MAX_TRACE_COLS];
                char mf[500];
                u32 numcols = cvec_len(&tv->columns);
                struct trace_view_col *col_ptrs[MAX_TRACE_COLS] = {};
                for (u32 i = 0; i < numcols; i++) {
                    struct trace_view_col *c = (struct trace_view_col *)cvec_get(&tv->columns, i);
                    u32 sz = 0;
                    if (c->default_size <= 0)
                        sz = 10;
                    else
                        sz = c->default_size + 1;
                    total_sz += sz;

                    widths[i] = (float)sz * TEXT_BASE_WIDTH;
                    col_ptrs[i] = c;
                }
                ImVec2 outer_size = ImVec2(TEXT_BASE_WIDTH * (float)(total_sz + 5), TEXT_BASE_HEIGHT * 20);
                if (ImGui::BeginTable("tabley_table", (int)numcols, flags, outer_size)) {
                    ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
                    for (u32 c = 0; c < numcols; c++) {
                        struct trace_view_col *mc = (struct trace_view_col *)cvec_get(&tv->columns, c);
                        ImGui::TableSetupColumn(mc->name, ImGuiTableColumnFlags_None, widths[c]);
                    }
                    ImGui::TableHeadersRow();
                    ImGuiListClipper clipper;

                    if (update_dasm_scroll && tv->autoscroll) {
                        if (tv->display_end_top) ImGui::SetScrollY(0);
                        else {
                            float scrl = clipper.ItemsHeight * (tv->num_trace_lines);
                            float cur_scroll = ImGui::GetScrollY();
                            if ((cur_scroll > scrl) || (scrl < (cur_scroll + (clipper.ItemsHeight * 8))))
                                ImGui::SetScrollY(scrl);
                        }
                    }
                    clipper.Begin(tv->num_trace_lines);
                    while (clipper.Step()) {
                        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                            ImGui::TableNextRow();
                            auto *ln = trace_view_get_line(tv, row);
                            for (int col_num = 0; col_num < numcols; col_num++) {
                                struct trace_view_col *my_col = col_ptrs[col_num];
                                ImGui::TableSetColumnIndex(col_num);

                                struct jsm_string *mstr = &ln->cols[col_num];
                                ImGui::TableSetColumnIndex(col_num);
                                ImGui::Text("%s", mstr->ptr);
                            }
                        }
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::End();
        }
    }
}

void imgui_jsmooch_app::render_dbglog_views(bool update_dasm_scroll)
{
    u32 i = 0;
    for (auto &myv : fsys.dlviews) {
        struct managed_window *mw = register_managed_window(0x2500 + (i++), mwk_debug_dbglog, myv.view->dbglog.name, DBGLOG_VIEW_DEFAULT_ENABLE);
        if (mw->enabled) {
            render_dbglog_view(myv, update_dasm_scroll);
        }
    }
}

void imgui_jsmooch_app::render_image_views()
{
    u32 i=0;
    for (auto &myv: fsys.images) {
        struct managed_window *mw = register_managed_window(0x500 + (i++), mwk_debug_image, myv.view->image.label, IMAGE_VIEW_DEFAULT_ENABLE);
        if (mw->enabled) {
            if (ImGui::Begin(myv.view->image.label)) {
                render_debugger_widgets(&myv.view->options);
                fsys.image_view_present(myv.view, myv.texture);
                ImGui::Image(myv.texture.for_image(), myv.texture.sz_for_display, myv.texture.uv0, myv.texture.uv1);
                render_debugger_post_widgets(&myv.view->options);
            }
            ImGui::End();
        }
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

void imgui_jsmooch_app::render_debug_views(ImGuiIO& io, bool update_dasm_scroll)
{
    render_event_view();
    render_memory_view();
    render_disassembly_views(update_dasm_scroll);
    render_dbglog_views(update_dasm_scroll);
    render_image_views();
    render_trace_view(update_dasm_scroll);
    render_console_view(update_dasm_scroll);
    u32 i = 0;
    for (auto &wv : fsys.waveform_views) {
        render_waveform_view(wv, i++);
    }
    render_window_manager();
}

void imgui_jsmooch_app::render_window_manager()
{
    if (windows.num == 0) return;
    enum managed_window_kind old_mwk = mwk_debug_events;
    if (ImGui::Begin("Window Manager")) {
        for (u32 i = 0; i < windows.num; i++) {
            struct managed_window *mw = &windows.items[i];
            if (mw->kind != old_mwk) {
                old_mwk = mw->kind;
                ImGui::Separator();
            }
            bool mval = mw->enabled ? true : false;
            ImGui::Checkbox(mw->name, &mval);
            mw->enabled = mval ? 1 : 0;
        }
    }
    ImGui::End();
}

void imgui_jsmooch_app::do_setup_onstart()
{
    load_inifile(&ini);

    debugger_cols[0] = (char *)malloc(debugger_col_sizes[0]);
    debugger_cols[1] = (char *)malloc(debugger_col_sizes[1]);
    debugger_cols[2] = (char *)malloc(debugger_col_sizes[2]);

    struct kv_pair *kvp = inifile_get_or_make_key(&ini, "general", "bios_base_path");
    assert(kvp);
    snprintf(BIOS_BASE_PATH, sizeof(BIOS_BASE_PATH), "%s", kvp->str_value);

    kvp = inifile_get_or_make_key(&ini, "general", "rom_base_path");
    assert(kvp);
    snprintf(BIOS_BASE_PATH, sizeof(ROM_BASE_PATH), "%s", kvp->str_value);
}

struct RenderResources
{
    WGPUTexture         FontTexture = nullptr;          // Font texture
    WGPUTextureView     FontTextureView = nullptr;      // Texture view for font texture
    WGPUSampler         Sampler = nullptr;              // Sampler for the font texture
    WGPUBuffer          Uniforms = nullptr;             // Shader uniforms
    WGPUBindGroup       CommonBindGroup = nullptr;      // Resources bind-group to bind the common resources to pipeline
    ImGuiStorage        ImageBindGroups;                // Resources bind-group to bind the font/image resources to pipeline (this is a key->value map)
    WGPUBindGroup       ImageBindGroup = nullptr;       // Default font-resource of Dear ImGui
    WGPUBindGroupLayout ImageBindGroupLayout = nullptr; // Cache layout used for the image bind group. Avoids allocating unnecessary JS objects when working with WebASM
};

#ifdef JSM_WEBGPU
void imgui_jsmooch_app::setup_wgpu()
{
    /*ImGui_ImplWGPU_RenderState *rs = (ImGui_ImplWGPU_RenderState*)ImGui::GetPlatformIO().Renderer_RenderState;
    rs->RenderPassEncoder.
    WGPUSamplerDescriptor sampler_desc = {};
    sampler_desc.minFilter = WGPUFilterMode_Linear;
    sampler_desc.magFilter = WGPUFilterMode_Linear;
    sampler_desc.mipmapFilter = WGPUMipmapFilterMode_Linear;
    sampler_desc.addressModeU = WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeW = WGPUAddressMode_ClampToEdge;
    sampler_desc.maxAnisotropy = 1;
    my_sampler = wgpuDeviceCreateSampler(bd->wgpuDevice, &sampler_desc);
    */
}
#endif

int imgui_jsmooch_app::do_setup_before_mainloop()
{
#ifdef DO_DREAMCAST
    which = SYS_DREAMCAST;
#else
    //which = SYS_ATARI2600;
    //which = SYS_GBC;
    //which = SYS_APPLEIIe;
    //which = SYS_DMG;
    //which = SYS_PS1;
    //which = SYS_SMS2;
    //which = SYS_GG;
    //which = SYS_ZX_SPECTRUM_48K;
    //which = SYS_ZX_SPECTRUM_128K;
    //which = SYS_SG1000;
    //which = SYS_MAC512K;
    //which = SYS_GBA;
    //which = SYS_SNES;
    //which = SYS_GENESIS_USA;
    //which = SYS_MEGADRIVE_PAL;
    //which = SYS_NDS;
    which = SYS_TURBOGRAFX16;
    //which = SYS_NES;
    //which = SYS_GALAKSIJA;
    //dbg_enable_trace();
#endif

    fsys.setup_system(which);
    if (!fsys.worked) {
        printf("\nCould not initialize system! %d", fsys.worked);
        return -1;
    }
    fsys.state = FSS_pause;
    fsys.has_played_once = false;
#ifdef JSM_WEBGPU
    setup_wgpu();
#endif
    fsys.setup_wgpu();

    fsys.setup_tracing();
    return 0;
}

struct managed_window *imgui_jsmooch_app::register_managed_window(u32 id, enum managed_window_kind kind, const char *name, u32 default_enabled)
{
    if (windows.num > 0) {
        for (u32 i = 0; i < windows.num; i++) {
            struct managed_window *mw = &windows.items[i];
            if (mw->id == id) {
                return mw;
            }
        }
    }
    struct managed_window *out = &windows.items[windows.num];
    windows.num++;
    out->id = id;
    out->kind = kind;
    snprintf(out->name, sizeof(out->name), "%s", name);
    out->enabled = default_enabled;
    return out;
}

void imgui_jsmooch_app::mainloop(ImGuiIO& io) {
    framevars start_fv = fsys.get_framevars();

    fsys.enable_debugger = true;

    // Update the system
    if (dbg.do_break) {
        if (fsys.state == FSS_play)
            fsys.state = FSS_pause;
    }

    static u32 hotkeys[2];
    update_input(&fsys, hotkeys, io);
    if (hotkeys[0]) {
        fsys.save_state();
    }
    if (hotkeys[1]) {
        fsys.load_state();
    }
    static u32 key_was_down = 0;
    u32 frame_multi = ImGui::IsKeyDown(ImGuiKey_GraveAccent) ? FRAME_MULTI : 1;
    if (fsys.state == FSS_play) {
        if (key_was_down && (frame_multi == 1)) {
            // Discard audio buffers!
            fsys.discard_audio_buffers();
        }
        fsys.advance_time(0, 0, frame_multi);
        last_frame_was_whole = true;
        //fsys.events_view_present();
        fsys.has_played_once = true;
    }
    key_was_down = frame_multi != 1;
    fsys.present();

    render_emu_window(fsys, io, frame_multi);
    // debug controls window
    if (ImGui::Begin("Play")){
        static int steps[4] = { 100, 1, 1, 1 };
        bool play_pause = false;
        bool screenshot = false;
        bool step_clocks = false, step_scanlines = false, step_seconds = false, step_frames = false;
        bool trace_enabled = dbg.trace_on;
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
            step_frames = ImGui::Button("Step");
            ImGui::PopID();
            ImGui::SameLine();
            ImGui::InputInt("frames", &steps[2]);

            ImGui::PushID(4);
            step_seconds = ImGui::Button("Step");
            ImGui::PopID();
            ImGui::SameLine();
            ImGui::InputInt("seconds", &steps[3]);

            ImGui::PushID(5);
            ImGui::Checkbox("Trace enabled", &trace_enabled);
            ImGui::PopID();
            if (dbg.trace_on != trace_enabled) {
                if (trace_enabled) {
                    dbg_enable_trace();
                }
                else {
                    dbg_disable_trace();
                }
            }
            fsys.screenshot = ImGui::Button("Screenshot");
            if (screenshot) {

            }
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
            fsys.advance_time(0, steps[1], 0);
            //fsys.step_scanlines(steps[1]);
            last_frame_was_whole = false;
        }
        if (step_clocks) {
            dbg_unbreak();
            //printf("\nCYCLING %d", steps[0]);
            //fsys.step_cycles(steps[0]);
            fsys.advance_time(steps[0], 0, 0);

            last_frame_was_whole = false;
        }
        if (step_frames) {
            dbg_unbreak();
            //fsys.step_frames(steps[2]);
            fsys.advance_time(0, 0, steps[2]);
            last_frame_was_whole = true;
        }
        /*if (step_seconds) {
            dbg_unbreak();
            fsys.step_seconds(steps[3]);
            last_frame_was_whole = false;
        }*/

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
    render_debug_views(io, update_dasm_scroll);
}

void imgui_jsmooch_app::at_end()
{
    fsys.destroy_system();

    inifile_delete(&ini);
}

#ifdef JSM_OPENGL
int opengl_main(imgui_jsmooch_app &app)
{
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

#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    int a = app.do_setup_before_mainloop();
    if (a) {
        return a;
    }
    while (!app.done)
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        //io.WantCaptureKeyboard = false;
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                app.done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                app.done = true;
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

        app.mainloop(io);

        // Rendering
        ImGui::Render();
        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
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

    app.at_end();
    return 0;
}
#endif

#ifdef JSM_SDLR3
static int sldr3_main(imgui_jsmooch_app &app) {
    // Setup SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return -1;
    }
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "vulkan");
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, 0);

    // Create window with SDL_Renderer graphics context
    Uint32 window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
    SDL_Window* window = SDL_CreateWindow("JSMooCh GUI", 1280, 720, window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return -1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    app.platform_setup(renderer);
    SDL_SetRenderVSync(renderer, 1);
    if (renderer == nullptr)
    {
        SDL_Log("Error: SDL_CreateRenderer(): %s\n", SDL_GetError());
        return -1;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    int a = app.do_setup_before_mainloop();
    if (a) {
        return a;
    }
    while (!app.done)
#ifdef __EMSCRIPTEN__
        // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
        while (!app.done)
#endif
        {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        //io.WantCaptureKeyboard = false;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                app.done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                app.done = true;
        }
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }

        // React to changes in screen size
        int width, height;
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        app.mainloop(io);

        // Rendering
        ImGui::Render();
        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
            SDL_SetRenderDrawColorFloat(renderer, clear_color.x, clear_color.y, clear_color.z, clear_color.w);
            SDL_RenderClear(renderer);
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
            SDL_RenderPresent(renderer);
        }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif
    // Cleanup
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    app.at_end();
    return 0;

}
#endif

#ifdef JSM_WEBGPU
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

// Main code
int webgpu_main(imgui_jsmooch_app &app)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Make sure GLFW does not initialize any graphics context.
    // This needs to be done explicitly later.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_SRGB_CAPABLE , GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(wgpu_swap_chain_width, wgpu_swap_chain_height, "JSMooCh GUI!", nullptr, nullptr);
    if (window == nullptr)
        return 1;

    // Initialize the WebGPU environment
    if (!InitWGPU(window))
    {
        if (window)
            glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    app.platform_setup(wgpu_device);

    CreateSwapChain(wgpu_swap_chain_width, wgpu_swap_chain_height);
    glfwShowWindow(window);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;


    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
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

    int a = app.do_setup_before_mainloop();
    if (a) {
        return a;
    }


    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!glfwWindowShouldClose(window) && !app.done)
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
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

        app.mainloop(io);

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

    // Cleanup
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

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

#endif

// Main code
int main(int, char**)
{
    imgui_jsmooch_app app;
    app.do_setup_onstart();

#ifdef JSM_WEBGPU
    return webgpu_main(app);
#endif

#ifdef JSM_OPENGL
    return opengl_main(app);
#endif

#ifdef JSM_METAL
    return metal_main(app);
#endif

#ifdef JSM_SDLR3
    return sldr3_main(app);
#endif
}

