#include "helpers/int.h"
#include "helpers/sys_interface.h"
#include "helpers/physical_io.h"
#include "helpers/debugger/debugger.h"


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

struct full_system {
public:
    struct jsm_system *sys;
    struct debugger_interface dbgr{};
    struct system_io inputs;
    struct cvec dasm_rows{};
    u32 worked;

    struct disassembly_view *dasm{};

    full_system() {
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

    void setup_system(enum jsm_systems which);
    void destroy_system();
    void do_frame() const;
    void present(void *outptr, u32 out_width, u32 out_height) const;
    struct framevars get_framevars() const;
};

void newsys(struct full_system *fsys);



