//
// Created by . on 12/28/24.
//

#ifndef JSMOOCH_EMUS_TRACE_H
#define JSMOOCH_EMUS_TRACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "debugger.h"

void trace_view_init(trace_view *);
void trace_view_delete(trace_view *);

#ifdef __cplusplus
}
#endif

#endif //JSMOOCH_EMUS_TRACE_H
