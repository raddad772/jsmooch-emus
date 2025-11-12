#pragma once

#include <helpers/sys_interface.h>
#include <helpers/enums.h>
#include <helpers/physical_io.h>
#include <helpers/audiobuf.h>
#include <helpers/serialize/serialize.h>
#include <vector>

#include "debugger/debugger.h"

struct framevars {
    u64 master_frame{};
    i32 x{};
    u32 scanline{};
    u32 last_used_buffer{};
    u64 master_cycle{};
};

struct deserialize_ret {
    char reason[50]{};
    u32 success{};
};

jsm_system* new_system(jsm::systems which);

struct jsm_system {
public:
    virtual ~jsm_system() = default;

    char label[100]{};

    jsm::systems kind{}; // Which system is it?
    virtual u32 finish_frame() { return 0; };
    virtual u32 finish_scanline() {return 0; };
    virtual u32 step_master(u32) { return 0; };
    virtual void reset() {};
    virtual void load_BIOS(multi_file_set& mfs) {};
    virtual void describe_io() {};
    virtual void get_framevars(framevars& out) {};

    virtual void sideload(multi_file_set& mfs) {};

    virtual void set_audiobuf(audiobuf *ab) {};
    virtual void play() {};
    virtual void pause() {};
    virtual void stop() {};

    virtual void setup_debugger_interface(debugger_interface &intf) {};

    struct {
        bool save_state=false, load_BIOS=false, set_audiobuf=false;
    } has{};

    virtual void save_state(serialized_state &state) {};
    virtual void load_state(serialized_state &state, deserialize_ret &ret) {};

    std::vector<physical_io_device> IOs{};
    std::vector<debugger_widget> opts{};
};
