//
// Created by . on 8/2/24.
//

#ifndef JSMOOCH_EMUS_OOC_H
#define JSMOOCH_EMUS_OOC_H

#include "helpers/cvec.h"

// Stack of destructors to call

enum ooc_context_kind
{
    oc_function,
};

struct ooc_context_frame {
    struct cvec *parent;
    u32 index;
};


struct ooc_context {
    struct cvec storage;
};

#endif //JSMOOCH_EMUS_OOC_H
