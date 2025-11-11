//
// Created by . on 6/3/25.
//

#ifndef JSMOOCH_EMUS_MEMORY_H
#define JSMOOCH_EMUS_MEMORY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "debugger.h"

void memory_view_init(memory_view *);
void memory_view_delete(memory_view *);


#ifdef __cplusplus
}
#endif

#endif //JSMOOCH_EMUS_MEMORY_H
