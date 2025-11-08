#ifndef JSMOOCH_SYS_INTERFACE_CPP_H
#define JSMOOCH_SYS_INTERFACE_CPP_H
#include <helpers/sys_interface.h>
#include <helpers/enums.h>
#include <helpers/physical_io.h>
#include <vector>

#include "debugger/debugger.h"

struct jsm_system {
    virtual ~jsm_system() = default;

    char label[100]{};

    jsm::systems kind{}; // Which system is it?
    virtual u32 finish_frame() { return 0; };
    virtual u32 finish_scanline() {return 0; };
    virtual u32 step_master(u32) { return 0; };
    virtual void reset() {};
    virtual void load_BIOS(struct multi_file_set* mfs) {};
    virtual void describe_io(std::vector<physical_io_device> &inIOs) {};
    virtual void get_framevars(struct framevars* out) {};

    virtual void sideload(struct multi_file_set* mfs) {};

    virtual void set_audiobuf(struct audiobuf *ab) {};
    virtual void play() {};
    virtual void pause() {};
    virtual void stop() {};

    virtual void setup_debugger_interface(struct debugger_interface *intf) {};

    virtual void save_state(struct serialized_state &state) {};
    virtual void load_state(struct serialized_state &state, struct deserialize_ret &ret) {};

    std::vector<physical_io_device> *IOs{};
    std::vector<debugger_widget> *opts{};
};


#endif