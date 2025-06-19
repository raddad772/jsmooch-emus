//
// Created by . on 6/18/25.
//

#include <string.h>

#include "huc6270.h"

void HUC6270_init(struct HUC6270 *this)
{
    memset(this, 0, sizeof(*this));
}


void HUC6270_delete(struct HUC6270 *this)
{
}

void HUC6270_reset(struct HUC6270 *this)
{

}

static void new_line(struct HUC6270 *this)
{
    this->regs.yscroll = this->regs.next_yscroll;
    this->regs.next_yscroll = this->regs.yscroll + 1;

}

static void new_frame(struct HUC6270 *this)
{
    this->regs.y = -1; // return this +64
    this->regs.next_yscroll = this->io.BYR;
}

// on write BYR, set next_scroll also
