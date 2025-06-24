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
    this->regs.MPR[7] = 0;
}

void HUC6280_setup_tracing(struct HUC6280* this, struct jsm_debug_read_trace *strct)
{
    this->trace.strct.ptr = this;
    this->trace.strct.read_trace = strct->read_trace;
    this->trace.ok = 1;
}

void HUC6280_poll_NMI_only(struct HUC6280_regs *regs, struct HUC6280_pins *pins)
{
    if (regs->NMI_level_detected) {
        regs->do_NMI = 1;
        regs->NMI_level_detected = 0;
    }
}

// Poll during second-to-last cycle
void HUC6280_poll_IRQs(struct HUC6280_regs *regs, struct HUC6280_pins *pins)
{
    if (regs->NMI_level_detected) {
        regs->do_NMI = 1;
        regs->NMI_level_detected = 0;
    }

    regs->do_IRQ = pins->IRQ && !regs->P.I;
}
