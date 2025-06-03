//
// Created by . on 6/3/25.
//

#include "memory.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "helpers/cvec.h"
#include "helpers/ooc.h"
#include "helpers/physical_io.h"
#include "events.h"

void memory_view_init(struct memory_view *mv)
{
    memset(mv, 0, sizeof(*mv));
    cvec_init(&mv->modules, sizeof(struct memory_view_module), 50);
}

void memory_view_delete(struct memory_view *mv)
{
    cvec_delete(&mv->modules);
}

