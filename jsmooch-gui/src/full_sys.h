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
        struct HID_digital_button* up;
        struct HID_digital_button* down;
        struct HID_digital_button* left;
        struct HID_digital_button* right;
        struct HID_digital_button* fire1;
        struct HID_digital_button* fire2;
        struct HID_digital_button* fire3;
        struct HID_digital_button* fire4;
        struct HID_digital_button* fire5;
        struct HID_digital_button* fire6;
        struct HID_digital_button* shoulder_left;
        struct HID_digital_button* shoulder_right;
        struct HID_digital_button* start;
        struct HID_digital_button* select;
    } p[2]{};
    struct kb {
        //SDL_KeyCode
        //struct HID_digital_button* 1
    };
    struct HID_digital_button* ch_power;
    struct HID_digital_button* ch_reset;
    struct HID_digital_button* ch_pause;
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
    struct my_texture tex{};
    bool enabled{};
    struct debug_waveform *wf{};
    bool output_enabled{true};
    u32 height{};
    std::vector<u8> drawbuf{};
};

class WVIEW {
public:
    struct waveform_view *view{};
    std::vector<class WFORM> waveforms{};
};

class IVIEW {
public:
    bool enabled{};
    struct debugger_view *view{};
    struct my_texture texture;
};

class DVIEW {
public:
    struct debugger_view *view{};
    struct cvec dasm_rows{};
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
    struct jsm_system *sys;
    struct debugger_interface dbgr{};

    struct multi_file_set ROMs;
    std::vector<WVIEW> waveform_views;
    std::vector<DVIEW> dasm_views;

    struct system_io inputs;
    std::vector<struct JSM_AUDIO_CHANNEL *> audiochans;
    bool has_played_once{};
    bool enable_debugger{};
    u32 worked;

    struct audiowrap audio{};

    full_system() {
        sys = nullptr;
        debugger_interface_init(&dbgr);
        //cvec_ptr_init(&dasm);
        worked = 0;
        state = FSS_pause;
    }

    ~full_system();
    enum full_system_states state;

    struct fsio {
        struct cvec_ptr controller1{};
        struct cvec_ptr controller2{};
        struct cvec_ptr display{};
        struct cvec_ptr chassis{};
        struct cvec_ptr keyboard{};
        struct cvec_ptr mouse{};
        struct cvec_ptr cartridge_port{};
        struct cvec_ptr disk_drive{};
        struct cvec_ptr audio_cassette{};

        fsio() = default;
    } io{};

    struct {
        float u_overscan, v_overscan;
        struct fssothing with_overscan;
        struct fssothing without_overscan;
        struct JSM_DISPLAY *display;
        struct my_texture backbuffer_texture;
        void *backbuffer_backer{};

        double x_scale_mult, y_scale_mult;

        bool zoom, hide_overscan;
    } output{};

    struct {
        struct my_texture texture;
        struct events_view *view{};
    } events;

    std::vector<IVIEW> images;

    [[nodiscard]] ImVec2 output_size() const;
    [[nodiscard]] ImVec2 output_uv0() const;
    [[nodiscard]] ImVec2 output_uv1() const;
    void setup_persistent_store(struct persistent_store *ps, struct multi_file_set *mfs);
    void sync_persistent_storage();
    struct persistent_store *my_ps{};
    void setup_system(enum jsm_systems which);
    void destroy_system();
    void save_state();
    void load_state();
    void do_frame();
    struct framevars get_framevars() const;
    void present();
    void events_view_present();
    void pre_events_view_present();
    void waveform_view_present(struct WVIEW &wv);
    void image_view_present(struct debugger_view *dview, struct my_texture &tex);
    void setup_wgpu();
    void setup_audio();
    void step_seconds(int num);
    void step_scanlines(int num);
    void step_cycles(int num);
    void step_frames(int num);
private:
    void debugger_pre_frame();
    void debugger_pre_frame_waveforms(struct waveform_view *wv);
    void setup_ios();
    void load_default_ROM();
    void setup_bios();
    void get_savestate_filename(char *pth, size_t sz);
    void setup_display();
    void setup_debugger_interface();
    void add_disassembly_view(u32);
    void add_image_view(u32);
    void add_waveform_view(u32 idx);
    void waveform_view_present(struct debugger_view *dview, struct WFORM &wf);
};

void newsys(struct full_system *fsys);



