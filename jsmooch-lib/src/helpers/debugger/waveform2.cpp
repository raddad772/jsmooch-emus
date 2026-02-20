//
// Created by . on 2/20/26.
//

#include "debugger.h"
#include "waveform2.h"
namespace debug::waveform2 {
view_node *view_node::add_child_wf(kinds kind_in, cvec_ptr<view_node> &wfptr)
{
    auto &b = children.emplace_back();
    wfptr.make(children, children.size()-1);
    data.kind = kind_in;
    switch (data.kind) {
        case wk_big:
            data.samples_requested = 400;
            break;
        case wk_medium:
            data.samples_requested = 200;
            break;
        case wk_small:
            data.samples_requested = 100;
            break;
    }

    return &b;
}

view_node &view_node::add_child_category(const char *name_in, u32 num_reserve) {
    auto &b = children.emplace_back();
    snprintf(b.name, sizeof(b.name), "%s", name_in);
    b.children.reserve(num_reserve);
    return b;
}
}