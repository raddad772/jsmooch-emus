//
// Created by . on 6/12/25.
//

#include "huc6280.h"


void HUC6280_init(struct HUC6280 *this)
{

}

void HUC6280_delete(struct HUC6280 *this)
{

}

void HUC6280_reset(struct HUC6280 *this)
{

}

void HUC6280_setup_tracing(struct HUC6280* this, struct jsm_debug_read_trace *strct)
{
    this->trace.strct.ptr = this;
    this->trace.strct.read_trace = strct->read_trace;
    this->trace.ok = 1;
}