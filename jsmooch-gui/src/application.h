#include "full_sys.h"
#include "helpers/int.h"
#include "helpers/inifile.h"

struct imgui_jsmooch_app {
    void do_setup_onstart();
    int do_setup_before_mainloop();
    void mainloop(ImGuiIO& io);
    void at_end();

    int done_break = 0;
    bool last_frame_was_whole = false;
    bool playing = true;
    bool done = false;
    full_system fsys;
    enum jsm_systems which;
    struct inifile ini;

    char BIOS_BASE_PATH[500];
    char ROM_BASE_PATH[500];
    char *debugger_cols[3];
    constexpr static size_t debugger_col_sizes[3] = { 10 * 1024, 100 * 1024, 500 * 1024 };

};