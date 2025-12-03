//
// Created by . on 12/3/25.
//

#include "mac_via.h"
#include "mac_bus.h"

namespace mac {
void via::reset()
{
    state.t1_active = state.t2_active = 0;
}

void via::irq_sample()
{
    u32 old_irq = bus->io.irq.via;
    bus->io.irq.via = (regs.IER & regs.IFR & 0x7F) != 0;
    if (old_irq != bus->io.irq.via) bus->set_cpu_irq();
}

void via::step()
{
    irq_sample();

    // TODO: add .5 cycles to the countdowns
    u32 IRQ_sample = 0;
    if (state.t1_active) {
        // 7=PB out
        // 6=1 is continuous, 6=0 is one-shot
        if (regs.T1C == 0) {
            // PB7 gets modified if
            if (((regs.ACR & regs.dirB) >> 7) & 1) {
                bus->set_sound_output(1);
            }
            regs.IFR |= 0x40;
            IRQ_sample = 1;

            if ((regs.ACR >> 6) & 1) {
                regs.T1C = regs.T1L;
            }
            else {
                state.t1_active = 0;
            }
        }
        else regs.T1C--;
    }
    if (state.t2_active) {
        if (regs.T2C == 0) {
            regs.IFR |= 0x20;
            IRQ_sample = 1;
            state.t2_active = 0;
        }
    }
    regs.T2C--; // T2C counts down no matter what
    if (IRQ_sample) irq_sample();
}

    
}