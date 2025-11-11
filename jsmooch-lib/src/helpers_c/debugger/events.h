//
// Created by . on 8/11/24.
//

#ifndef JSMOOCH_EMUS_EVENTS_H
#define JSMOOCH_EMUS_EVENTS_H

#include "debugger.h"

void events_view_init(events_view *);
void events_view_delete(events_view *);
void events_view_report_line(events_view *this, i32 line_num);
void events_view_report_frame(events_view *this);


#endif //JSMOOCH_EMUS_EVENTS_H
