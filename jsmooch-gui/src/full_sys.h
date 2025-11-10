#pragma once

#include <vector>
#include "build.h"

#include "helpers/int.h"
#include "helpers/sys_interface.h"
#include "helpers/physical_io.h"
#include "helpers/debugger/debugger.h"
#include "helpers/cvec.h"

#include "my_texture.h"

#include "audiowrap.h"


#define MAX_IMAGE_VIEWS 5

struct system_io {
    system_io() {
        for (auto & i : p) {
            i.up = i.down = i.left = i.right = nullptr;
            i.fire1 = i.fire2 = i.fire3 = nullptr;
            i.fire4 = i.fire5 = i.fire6 = nullptr;
            i.shoulder_right = i.shoulder_left = nullptr;
            i.start = i.select = nullptr;
        }
        ch_power = ch_reset = ch_pause = nullptr;
    }
    struct CDKRKR {
        HID_digital_button* up;
        HID_digital_button* down;
        HID_digital_button* left;
        HID_digital_button* right;
        HID_digital_button* fire1;
        HID_digital_button* fire2;
        HID_digital_button* fire3;
        HID_digital_button* fire4;
        HID_digital_button* fire5;
        HID_digital_button* fire6;
        HID_digital_button* shoulder_left;
        HID_digital_button* shoulder_right;
        HID_digital_button* start;
        HID_digital_button* select;
    } p[2]{};
    struct kb {
        //SDL_KeyCode
        //HID_digital_button* 1
    };
    HID_digital_button* ch_power;
    HID_digital_button* ch_reset;
    HID_digital_button* ch_pause;
};

enum full_system_states {
    FSS_pause,
    FSS_play
};

struct fssothing {
    ImVec2 uv0, uv1;
    float x_size, y_size;
};

class WFORM {
public:
    my_texture tex{};
    bool enabled{};
    debug_waveform *wf{};
    bool output_enabled{true};
    u32 height{};
    std::vector<u8> drawbuf{};
};

class WVIEW {
public:
    waveform_view *view{};
    std::vector<class WFORM> waveforms{};
};

class CVIEW {
public:
    bool enabled{};
    debugger_view *view;
};

class TVIEW {
public:
    bool enabled{};
    debugger_view *view;
};

class IVIEW {
public:
    bool enabled{};
    debugger_view *view{};
    my_texture texture;
};

class DLVIEW {
public:
    bool enabled{};
    debugger_view *view{};
};

class DVIEW {
public:
    debugger_view *view{};
    std::vector<disassembly_entry_strings> dasm_rows{};
};

struct full_system {
public:
#ifdef JSM_SDLR3
    SDL_Renderer *renderer;
    void platform_setup(SDL_Renderer *mrenderer) {
        renderer = mrenderer;
    }
#endif
#ifdef JSM_WEBGPU
    WGPUDevice wgpu_device{};
    void platform_setup(WGPUDevice device) { wgpu_device = device; };
#endif
    jsm_system *sys;
    debugger_interface dbgr{};

    multi_file_set ROMs;
    std::vector<WVIEW> waveform_views;
    std::vector<DVIEW> dasm_views;
    std::vector<TVIEW> trace_views;
    std::vector<CVIEW> console_views;
    bool screenshot;
    system_io inputs;
    std::vector<JSM_AUDIO_CHANNEL *> audiochans;
    bool has_played_once{};
    bool enable_debugger{};
    u32 worked;

    audiowrap audio{};

    u32 debugger_setup{};

    full_system() {
        sys = nullptr;
        //cvec_ptr_init(&dasm);
        worked = 0;
        state = FSS_pause;
    }

    ~full_system();
    full_system_states state;

    struct fsio {
        JSM_TOUCHSCREEN *touchscreen{};
        JSM_CONTROLLER *controller1{};
        JSM_CONTROLLER *controller2{};
        JSM_DISPLAY *display{};
        JSM_CHASSIS *chassis{};
        JSM_KEYBOARD *keyboard{};
        JSM_MOUSE *mouse{};
        JSM_CARTRIDGE_PORT *cartridge_port{};
        JSM_DISC_DRIVE *disk_drive{};
        JSM_AUDIO_CASSETTE *audio_cassette{};

        fsio() = default;
    } io{};

    struct {
        float u_overscan, v_overscan;
        fssothing with_overscan;
        fssothing without_overscan;
        JSM_DISPLAY *display;
        my_texture backbuffer_texture;
        void *backbuffer_backer{};

        double x_scale_mult, y_scale_mult;

        bool zoom, hide_overscan;
    } output{};

    struct {
        memory_view *view{};
    } memory;
    struct {
        my_texture texture;
        events_view *view{};
    } events;

    std::vector<IVIEW> images;
    std::vector<DLVIEW> dlviews;

    [[nodiscard]] ImVec2 output_size() const;
    [[nodiscard]] ImVec2 output_uv0() const;
    [[nodiscard]] ImVec2 output_uv1() const;
    void setup_persistent_store(persistent_store *ps, multi_file_set *mfs);
    void sync_persistent_storage();
    void update_touch(i32 x, i32 y, i32 button_down);
    persistent_store *my_ps{};
    void setup_system(enum jsm::systems which);
    void destroy_system();
    void save_state();
    void load_state();
    void do_frame();

    void check_new_frame();
    void discard_audio_buffers();
    void advance_time(u32 cycles, u32 scanlines, u32 frames);
    struct {
        u64 cycles = {};
        u32 scanlines = {};
        u64 frames = 0xFFFFFFFFFFFFFFFF;
        bool has_audio_buf = {};
    } int_time = {};
    framevars get_framevars() const;
    void present();
    void take_screenshot(void *where, u32 buf_width, u32 buf_height);
    void events_view_present();
    void pre_events_view_present();
    void waveform_view_present(WVIEW &wv);
    void image_view_present(debugger_view *dview, my_texture &tex);
    void setup_wgpu();
    void setup_audio();
    void setup_tracing();
private:
    void debugger_pre_frame();
    void debugger_pre_frame_waveforms(waveform_view *wv);
    void setup_ios();
    void load_default_ROM();
    void setup_bios();
    void get_savestate_filename(char *pth, size_t sz);
    void setup_display();
    void setup_debugger_interface();
    void add_trace_view(u32);
    void add_dbglog_view(u32);
    void add_console_view(u32);
    void add_disassembly_view(u32);
    void add_image_view(u32);
    void add_waveform_view(u32 idx);
    void waveform_view_present(debugger_view *dview, WFORM &wf);
};

void newsys(full_system *fsys);



