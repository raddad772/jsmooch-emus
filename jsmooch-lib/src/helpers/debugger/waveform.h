//
// Created by . on 9/5/24.
//

#ifndef JSMOOCH_EMUS_WAVEFORM_H
#define JSMOOCH_EMUS_WAVEFORM_H

#include "../int.h"
#include "debugger.h"

void waveform_view_init(struct waveform_view *);
void waveform_view_delete(struct waveform_view *);

void debug_waveform_delete(struct debug_waveform *);

#endif //JSMOOCH_EMUS_WAVEFORM_H
