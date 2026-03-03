//
// Created by . on 5/1/24.
//

#include <cassert>

#include "helpers/debugger/debuggerdefs.h"
#include "helpers/debugger/debugger.h"

#include "sh4_debugger.h"
#include "sh4_interpreter.h"
#include "sh4_interrupts.h"

namespace SH4 {

void core::IRQ_set_highest_priority()
{
    interrupt_highest_priority = 0;
    for (u32 i = 0; i < SH4I_NUM; i++) {
        if (interrupt_map[i]->raised) {
            interrupt_highest_priority = interrupt_map[i]->priority;
            break;
        }
    }
}


void core::exec_interrupt()
{
    regs.SPC = regs.PC;
    regs.SSR = regs.SR_get();
    regs.SGR = regs.R[15];

    u32 old_SR = regs.SR_get();

    regs.SR_set(old_SR | (1 << 30) | (1 << 29) | (1 << 28));

    regs.PC = regs.VBR + 0x00000600;
}

void core::interrupt() {
    dbgloglog(trace.exception_id, DBGLS_TRACE, "IRQ PRIO:%d IMASK:%d", interrupt_highest_priority, regs.SR.IMASK);
        //dbg_printf("\nIRQ PRIO:%d IMASK:%d", interrupt_highest_priority, regs.SR.IMASK);
    //}

    IRQ_SOURCE* src = nullptr;
    for (u32 i = 0; i < SH4I_NUM; i++) {
        if (interrupt_map[i]->raised) {
            src = interrupt_map[i];
            break;
        }
    }
    assert(src);
    regs.INTEVT = src->intevt;
    if (src->source != IRQ_irl) src->raised = 0; // IRL is de-asserted externally
    IRQ_set_highest_priority();
    exec_interrupt();
}

// -1 if we're good. 0 if we're bad. +1 if we need to swap
static int compare_sh4i(const void *a, const void *b)
{
    auto *p1 = static_cast<const IRQ_SOURCE *>(a);
    auto *p2 = static_cast<const IRQ_SOURCE *>(b);
    if (p1->priority > p2->priority) return -1;
    if (p1->priority == p2->priority) {
        if (p1->sub_priority > p2->sub_priority) return -1;
        if (p1->sub_priority == p2->sub_priority) return 0;
        return 1;
    }
    return 1;
}

static void bubble_sort(void *ptrs[], u8 sz, int (*compare_func)(const void *a, const void *b))
{
    // Note: bubble sort is optimal for already-sorted lists. So for our purposes it should be OK
    u32 swapped;
    void *ptr_tmp;
    do {
        swapped = 0;
        for (u32 i = 0; i < sz-1; i++) {
            i32 c = compare_func(ptrs[i], ptrs[i+1]);
            if (c > 0) {
                ptr_tmp = ptrs[i+1];
                ptrs[i+1] = ptrs[i];
                ptrs[i] = ptr_tmp;
                swapped = 1;
            }
        }
    } while(swapped);
}

void core::sort_interrupts()
{
    bubble_sort(reinterpret_cast<void **>(&interrupt_map), SH4I_NUM, &compare_sh4i);
}


void core::set_IRL(u32 level)
{
    interrupt_sources[IRQ_irl].priority = (~level) & 15;
    interrupt_sources[IRQ_irl].raised = (interrupt_sources[IRQ_irl].priority > 0) ? 1 : 0;
    interrupt_sources[IRQ_irl].intevt = 0x200 + (level * 0x20);
    sort_interrupts();
    IRQ_set_highest_priority();
}

// A write to ICR, IPRA, IPRB or IPRC
void core::IPR_update()
{
    //TODO if (regs.ICR)
#define SS(src, prior) interrupt_sources[src].priority = prior
    SS(IRQ_hitachi_udi, regs.IPRC.UDI);
    SS(IRQ_gpio_gpioi, regs.IPRC.GPIO);
    SS(IRQ_dmac_dmte0, regs.IPRC.DMAC);
    SS(IRQ_dmac_dmte1, regs.IPRC.DMAC);
    SS(IRQ_dmac_dmte2, regs.IPRC.DMAC);
    SS(IRQ_dmac_dmte3, regs.IPRC.DMAC);
    SS(IRQ_dmac_dmae, regs.IPRC.DMAC);
    SS(IRQ_tmu0_tuni0, regs.IPRA.TMU0);
    SS(IRQ_tmu1_tuni1, regs.IPRA.TMU1);
    SS(IRQ_tmu2_tuni2, regs.IPRA.TMU2);
    SS(IRQ_tmu2_ticpi2, regs.IPRA.TMU2);
    SS(IRQ_rtc_ati, regs.IPRA.RTC);
    SS(IRQ_rtc_pri, regs.IPRA.RTC);
    SS(IRQ_rtc_cui, regs.IPRA.RTC);
    SS(IRQ_sci1_eri, regs.IPRB.SCI1);
    SS(IRQ_sci1_rxi, regs.IPRB.SCI1);
    SS(IRQ_sci1_txi, regs.IPRB.SCI1);
    SS(IRQ_sci1_tei, regs.IPRB.SCI1);
    SS(IRQ_scif_eri, regs.IPRC.SCIF);
    SS(IRQ_scif_rxi, regs.IPRC.SCIF);
    SS(IRQ_scif_bri, regs.IPRC.SCIF);
    SS(IRQ_scif_txi, regs.IPRC.SCIF);
    SS(IRQ_wdt_iti, regs.IPRB.WDT);
    SS(IRQ_ref_rcmi, regs.IPRB.REF);
    SS(IRQ_ref_rovi, regs.IPRB.REF);
#undef SS
    sort_interrupts();
    IRQ_set_highest_priority();
}


void init_interrupt_struct(IRQ_SOURCE interrupt_sources[], IRQ_SOURCE* interrupt_map[])
{
#define SS(src, prior, sub_prior, intea) { interrupt_sources[src] = (IRQ_SOURCE) {.source = src, .priority = prior, .sub_priority = sub_prior, .intevt = intea }; interrupt_map[src] = &interrupt_sources[src]; }
    SS(IRQ_nmi, 16, 0, 0x1C0);
    SS(IRQ_irl, 1, 0, 0x200); // this will dynamically change
    SS(IRQ_hitachi_udi, 0, 0, 0x600);
    SS(IRQ_gpio_gpioi, 0, 0, 0x620);
    SS(IRQ_dmac_dmte0, 0, 4, 0x640);
    SS(IRQ_dmac_dmte1, 0, 3, 0x660);
    SS(IRQ_dmac_dmte2, 0, 2, 0x680);
    SS(IRQ_dmac_dmte3, 0, 1, 0x6A0);
    SS(IRQ_dmac_dmae, 0, 0, 0x6C0);
    SS(IRQ_tmu0_tuni0, 0, 0, 0x400);
    SS(IRQ_tmu1_tuni1, 0, 0, 0x420);
    SS(IRQ_tmu2_tuni2, 0, 4, 0x440);
    SS(IRQ_tmu2_ticpi2, 0, 0, 0x460);
    SS(IRQ_rtc_ati, 0, 2, 0x480);
    SS(IRQ_rtc_pri, 0, 1, 0x4A0);
    SS(IRQ_rtc_cui, 0, 0, 0x4C0);
    SS(IRQ_sci1_eri, 0, 3, 0x4E0);
    SS(IRQ_sci1_rxi, 0, 2, 0x500);
    SS(IRQ_sci1_txi, 0, 1, 0x520);
    SS(IRQ_sci1_tei, 0, 0, 0x540);
    SS(IRQ_scif_eri, 0, 3, 0x700);
    SS(IRQ_scif_rxi, 0, 2, 0x720);
    SS(IRQ_scif_bri, 0, 1, 0x740);
    SS(IRQ_scif_txi, 0, 0, 0x760);
    SS(IRQ_wdt_iti, 0, 0, 0x560);
    SS(IRQ_ref_rcmi, 0, 1, 0x580);
    SS(IRQ_ref_rovi, 0, 0, 0x5A0);
#undef SS
}


void core::interrupt_pend(IRQ_SOURCES source, u32 onoff)
{
    interrupt_sources[source].raised = onoff > 0 ? 1 : 0;
    IRQ_set_highest_priority();
};

}