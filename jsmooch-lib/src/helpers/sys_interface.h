#ifndef JSMOOCH_SYS_INTERFACE_CPP_H
#define JSMOOCH_SYS_INTERFACE_CPP_H
#include <helpers/sys_interface.h>

struct jsm_system_cpp {
    char label[100];

    enum jsm_systems kind; // Which system is it?
    virtual u32 finish_frame() { return 0; };
    virtual u32 finish_scanline() {return 0; };
    virtual u32 step_master(u32) { return 0; };
    virtual void reset() {};
    virtual void load_BIOS(struct multi_file_set* mfs) {};
    virtual void describe_io(struct cvec* IOs) {};
    virtual void get_framevars(struct framevars* out) {};

    virtual void sideload(struct multi_file_set* mfs) {};

    virtual void set_audiobuf(struct audiobuf *ab) {};
    virtual void play() {};
    virtual void pause() {};
    virtual void stop() {};

    virtual void setup_debugger_interface(struct debugger_interface *intf) {};

    virtual void save_state(struct serialized_state *state) {};
    virtual void load_state(struct serialized_state *state, struct deserialize_ret *ret) {};

    struct cvec IOs;
    struct cvec opts;
};


#endif