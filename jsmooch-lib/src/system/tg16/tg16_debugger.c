//
// Created by . on 6/18/25.
//

#include "tg16_debugger.h"

#define JTHIS struct TG16* this = (struct TG16*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct TG16* this
#define PAL_BOX_SIZE 10
#define PAL_BOX_SIZE_W_BORDER 11


void TG16J_setup_debugger_interface(JSM, struct debugger_interface *dbgr) {
    JTHIS;
}