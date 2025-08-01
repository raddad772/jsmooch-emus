//
// Created by . on 9/5/24.
//

#include <string.h>

#include "../ooc.h"
#include "waveform.h"

void waveform_view_init(struct waveform_view *this)
{
    this->name[0] = 0;
    cvec_init(&this->waveforms, sizeof(struct debug_waveform), 8);
}

void waveform_view_delete(struct waveform_view *this)
{
    this->name[0] = 0;
    DTOR_child_cvec(waveforms, debug_waveform);
}

void debug_waveform_init(struct debug_waveform *this)
{
    memset(this, 0, sizeof(struct debug_waveform));
    buf_init(&this->buf);
    buf_allocate(&this->buf, 8192);
    this->ch_output_enabled = 1;
}

void debug_waveform_delete(struct debug_waveform *this)
{
    this->name[0] = 0;
    buf_delete(&this->buf);
    this->samples_rendered = 0;
}
