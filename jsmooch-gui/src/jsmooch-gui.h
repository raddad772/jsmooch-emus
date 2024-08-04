#include "helpers/int.h"
#include "helpers/sys_interface.h"
#include "helpers/physical_io.h"
#include "system/gb/gb_enums.h"


struct system_io {
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
    } p[2];
    struct kb {
        //SDL_KeyCode
        //struct HID_digital_button* 1
    };
    struct HID_digital_button* ch_power;
    struct HID_digital_button* ch_reset;
    struct HID_digital_button* ch_pause;
};


struct full_system {
    struct jsm_system *sys;
    struct system_io inputs;

    u32 worked;

    struct physical_io_device* controller1;
    struct physical_io_device* controller2;
    struct physical_io_device* display;
    struct physical_io_device* chassis;
    struct physical_io_device* keyboard;
    struct physical_io_device* mouse;
    struct physical_io_device* cartridge_port;
    struct physical_io_device* disk_drive;
    struct physical_io_device* audio_cassette;
};


struct full_system setup_system(enum jsm_systems which);
void newsys(struct full_system *fsys);
void destroy_system(struct full_system *fsys);
void sys_do_frame(struct full_system *fsys);
void sys_present(struct full_system *fsys, void *outptr, u32 out_width, u32 out_height);
