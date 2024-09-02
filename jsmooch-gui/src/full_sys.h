#include <vector>

#include "helpers/int.h"
#include "helpers/sys_interface.h"
#include "helpers/physical_io.h"
#include "helpers/debugger/debugger.h"

#include "my_texture.h"

#define MAX_IMAGE_VIEWS 5

struct system_io {
    system_io() {
        for (auto & i : p) {
            i.up = i.down = i.left = i.right = nullptr;
            i.fire1 = i.fire2 = i.fire3 = nullptr;
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

class IVIEW {
public:
    bool enabled{};
    struct debugger_view *view{};
    struct my_texture texture;
};


struct full_system {
public:
    struct jsm_system *sys;
    struct debugger_interface dbgr{};
    struct system_io inputs;
    struct cvec dasm_rows{};
    bool has_played_once{};
    bool enable_debugger{};
    u32 worked;
    WGPUDevice        wgpu_device;

    struct disassembly_view *dasm{};


    full_system() {
        wgpu_device = nullptr;
        sys = nullptr;
        debugger_interface_init(&dbgr);
        //cvec_ptr_init(&dasm);
        cvec_init(&dasm_rows, sizeof(struct disassembly_entry_strings), 150);
        for (u32 i = 0; i < 200; i++) {
            auto *das = (struct disassembly_entry_strings *)cvec_push_back(&dasm_rows);
            memset(das, 0, sizeof(*das));
        }
        worked = 0;
        state = FSS_pause;
    }

    ~full_system() {
        debugger_interface_delete(&dbgr);
        destroy_system();
        cvec_delete(&dasm_rows);
    }

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
    void setup_system(enum jsm_systems which);
    void destroy_system();
    void do_frame() const;
    struct framevars get_framevars() const;
    void present();
    void events_view_present();
    void image_view_present(struct debugger_view *dview, struct my_texture &tex);
    void setup_wgpu();
    void step_seconds(int num);
    void step_scanlines(int num);
    void step_cycles(int num);
private:
    void setup_ios();
    void load_default_ROM();
    void setup_bios();
    void setup_display();
    void setup_debugger_interface();
    void add_image_view(u32);
};

void newsys(struct full_system *fsys);



