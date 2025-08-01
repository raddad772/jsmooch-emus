#ifndef JSMOOCH_SYS_INTERFACE_H
#define JSMOOCH_SYS_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "int.h"
#include "enums.h"
#include "buf.h"
#include "cvec.h"
#include "serialize/serialize.h"
#include "helpers/audiobuf.h"

enum JSM_filekind {
    ROM,
    BIOS,
    DISC_GDI
};

enum MD_timing {
    frame = 0,
    line = 1,
    timestep = 2
};

enum MD_display_standard {
    MD_NTSC = 0,
    MD_PAL = 1,
    MD_LCD
};

enum SCREENVAR_fields {
    current_frame = 0,
    current_scanline = 1,
    current_x = 2
};

struct input_map_keypoint {
    i32 player_num;
    char name[100]; // up, down, etc.
    u32 buf_pos;
    u32 internal_code;
};

struct overscan_info {
    u32 top;
    u32 bottom;
    u32 left;
    u32 right;
};

struct machine_description {
    char name[100];
    enum MD_timing timing;
    float fps;
    enum MD_display_standard display_standard;
    u32 x_resolution;
    u32 y_resolution;
    u32 xrw;
    u32 xrh;

    struct overscan_info overscan;

    void* out_ptr;
    u32 out_size;

    struct input_map_keypoint keymap[10];
    u32 keymap_size;
};

struct framevars {
    u64 master_frame;
    i32 x;
    u32 scanline;
    u32 last_used_buffer;
    u64 master_cycle;
    //dbg_info : debugger_info_t = new debugger_info_t();
    //console: string = '';
};

struct deserialize_ret {
    char reason[50];
    u32 success;
};

struct debugger_interface;
struct jsm_system {
    void* ptr; // Pointer that holds the system
    char label[100];

    enum jsm_systems kind; // Which system is it?

    u32 (*finish_frame)(struct jsm_system* jsm);
    u32 (*finish_scanline)(struct jsm_system* jsm);
    u32 (*step_master)(struct jsm_system* jsm, u32);
    void (*reset)(struct jsm_system* jsm);
    void (*load_BIOS)(struct jsm_system* jsm, struct multi_file_set* mfs);
    void (*describe_io)(struct jsm_system *jsm, struct cvec* IOs);
    void (*get_framevars)(struct jsm_system* jsm, struct framevars* out);

    void (*sideload)(struct jsm_system* jsm, struct multi_file_set* mfs);

    void (*set_audiobuf)(struct jsm_system* jsm, struct audiobuf *ab);
    void (*play)(struct jsm_system* jsm);
    void (*pause)(struct jsm_system* jsm);
    void (*stop)(struct jsm_system* jsm);

    void (*setup_debugger_interface)(struct jsm_system* jsm, struct debugger_interface *intf);

    void (*save_state)(struct jsm_system *jsm, struct serialized_state *state);
    void (*load_state)(struct jsm_system *jsm, struct serialized_state *state, struct deserialize_ret *ret);

    struct cvec IOs;
    struct cvec opts;
};

struct jsm_system* new_system(enum jsm_systems which);
void jsm_delete(struct jsm_system*);
void jsm_clearfuncs(struct jsm_system*);

#ifdef __cplusplus
}
#endif

#endif
