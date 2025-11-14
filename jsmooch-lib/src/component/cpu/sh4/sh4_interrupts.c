//
// Created by . on 5/1/24.
//

#include <cassert>
#include <cstdlib>
#include <cstdio>

#include "sh4_interrupts.h"


static void SH4_set_highest_priority(SH4* this)
{
    this->interrupt_highest_priority = 0;
    for (u32 i = 0; i < SH4I_NUM; i++) {
        if (this->interrupt_map[i]->raised) {
            this->interrupt_highest_priority = this->interrupt_map[i]->priority;
            break;
        }
    }
}


void SH4_exec_interrupt(SH4* this)
{
    this->regs.SPC = this->regs.PC;
    this->regs.SSR = SH4_regs_SR_get(&this->regs.SR);
    this->regs.SGR = this->regs.R[15];

    u32 old_SR = SH4_regs_SR_get(&this->regs.SR);

    SH4_SR_set(this, old_SR | (1 << 30) | (1 << 29) | (1 << 28));

    this->regs.PC = this->regs.VBR + 0x00000600;
}

void SH4_interrupt(SH4* this) {
    if (dbg.trace_on) {
        dbg_printf("\nIRQ PRIO:%d IMASK:%d", this->interrupt_highest_priority, this->regs.SR.IMASK);
    }

    struct SH4_interrupt_source* src = NULL;
    for (u32 i = 0; i < SH4I_NUM; i++) {
        if (this->interrupt_map[i]->raised) {
            src = this->interrupt_map[i];
            break;
        }
    }
    assert(src);
    this->regs.INTEVT = src->intevt;
#ifdef SH4_DBG_SUPPORT
    if (dbg.trace_on) {
        dbg_printf("\nRaising interrupt %d cyc:%llu", *this->trace.cycles);
    }
#endif
    if (src->source != sh4i_irl) src->raised = 0; // IRL is de-asserted externally
    SH4_set_highest_priority(this);
    SH4_exec_interrupt(this);
}

// -1 if we're good. 0 if we're bad. +1 if we need to swap
static int compare_sh4i(const void *a, const void *b)
{
    const struct SH4_interrupt_source *p1 = a;
    const struct SH4_interrupt_source *p2 = b;
    if (p1->priority > p2->priority) return -1;
    if (p1->priority == p2->priority) {
        if (p1->sub_priority > p2->sub_priority) return -1;
        if (p1->sub_priority == p2->sub_priority) return 0;
        return 1;
    }
    return 1;
}

static void bubble_sort(void *ptrs[], u32 sz, int (*compare_func)(const void *a, const void *b))
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

static void SH4_sort_interrupts(SH4* this)
{
    bubble_sort((void **)&this->interrupt_map, SH4I_NUM, &compare_sh4i);
}


void SH4_set_IRL_irq_level(SH4* this, u32 level)
{
    this->interrupt_sources[sh4i_irl].priority = (~level) & 15;
    this->interrupt_sources[sh4i_irl].raised = (this->interrupt_sources[sh4i_irl].priority > 0) ? 1 : 0;
    this->interrupt_sources[sh4i_irl].intevt = 0x200 + (level * 0x20);
    SH4_sort_interrupts(this);
    SH4_set_highest_priority(this);
}

// A write to ICR, IPRA, IPRB or IPRC
void SH4_IPR_update(SH4* this)
{
    //TODO if (this->regs.ICR)
#define SS(src, prior) this->interrupt_sources[src].priority = prior
    SS(sh4i_hitachi_udi, this->regs.IPRC.UDI);
    SS(sh4i_gpio_gpioi, this->regs.IPRC.GPIO);
    SS(sh4i_dmac_dmte0, this->regs.IPRC.DMAC);
    SS(sh4i_dmac_dmte1, this->regs.IPRC.DMAC);
    SS(sh4i_dmac_dmte2, this->regs.IPRC.DMAC);
    SS(sh4i_dmac_dmte3, this->regs.IPRC.DMAC);
    SS(sh4i_dmac_dmae, this->regs.IPRC.DMAC);
    SS(sh4i_tmu0_tuni0, this->regs.IPRA.TMU0);
    SS(sh4i_tmu1_tuni1, this->regs.IPRA.TMU1);
    SS(sh4i_tmu2_tuni2, this->regs.IPRA.TMU2);
    SS(sh4i_tmu2_ticpi2, this->regs.IPRA.TMU2);
    SS(sh4i_rtc_ati, this->regs.IPRA.RTC);
    SS(sh4i_rtc_pri, this->regs.IPRA.RTC);
    SS(sh4i_rtc_cui, this->regs.IPRA.RTC);
    SS(sh4i_sci1_eri, this->regs.IPRB.SCI1);
    SS(sh4i_sci1_rxi, this->regs.IPRB.SCI1);
    SS(sh4i_sci1_txi, this->regs.IPRB.SCI1);
    SS(sh4i_sci1_tei, this->regs.IPRB.SCI1);
    SS(sh4i_scif_eri, this->regs.IPRC.SCIF);
    SS(sh4i_scif_rxi, this->regs.IPRC.SCIF);
    SS(sh4i_scif_bri, this->regs.IPRC.SCIF);
    SS(sh4i_scif_txi, this->regs.IPRC.SCIF);
    SS(sh4i_wdt_iti, this->regs.IPRB.WDT);
    SS(sh4i_ref_rcmi, this->regs.IPRB.REF);
    SS(sh4i_ref_rovi, this->regs.IPRB.REF);
#undef SS
    SH4_sort_interrupts(this);
    SH4_set_highest_priority(this);
}


void SH4_init_interrupt_struct(SH4_interrupt_source interrupt_sources[], SH4_interrupt_source* interrupt_map[])
{
#define SS(src, prior, sub_prior, intea) { interrupt_sources[src] = (SH4_interrupt_source) {.source = src, .priority = prior, .sub_priority = sub_prior, .intevt = intea }; interrupt_map[src] = &interrupt_sources[src]; }
    SS(sh4i_nmi, 16, 0, 0x1C0);
    SS(sh4i_irl, 1, 0, 0x200); // this will dynamically change
    SS(sh4i_hitachi_udi, 0, 0, 0x600);
    SS(sh4i_gpio_gpioi, 0, 0, 0x620);
    SS(sh4i_dmac_dmte0, 0, 4, 0x640);
    SS(sh4i_dmac_dmte1, 0, 3, 0x660);
    SS(sh4i_dmac_dmte2, 0, 2, 0x680);
    SS(sh4i_dmac_dmte3, 0, 1, 0x6A0);
    SS(sh4i_dmac_dmae, 0, 0, 0x6C0);
    SS(sh4i_tmu0_tuni0, 0, 0, 0x400);
    SS(sh4i_tmu1_tuni1, 0, 0, 0x420);
    SS(sh4i_tmu2_tuni2, 0, 4, 0x440);
    SS(sh4i_tmu2_ticpi2, 0, 0, 0x460);
    SS(sh4i_rtc_ati, 0, 2, 0x480);
    SS(sh4i_rtc_pri, 0, 1, 0x4A0);
    SS(sh4i_rtc_cui, 0, 0, 0x4C0);
    SS(sh4i_sci1_eri, 0, 3, 0x4E0);
    SS(sh4i_sci1_rxi, 0, 2, 0x500);
    SS(sh4i_sci1_txi, 0, 1, 0x520);
    SS(sh4i_sci1_tei, 0, 0, 0x540);
    SS(sh4i_scif_eri, 0, 3, 0x700);
    SS(sh4i_scif_rxi, 0, 2, 0x720);
    SS(sh4i_scif_bri, 0, 1, 0x740);
    SS(sh4i_scif_txi, 0, 0, 0x760);
    SS(sh4i_wdt_iti, 0, 0, 0x560);
    SS(sh4i_ref_rcmi, 0, 1, 0x580);
    SS(sh4i_ref_rovi, 0, 0, 0x5A0);
#undef SS
}


void SH4_interrupt_pend(SH4* this, enum SH4_interrupt_sources source, u32 onoff)
{
    this->interrupt_sources[source].raised = onoff > 0 ? 1 : 0;
    SH4_set_highest_priority(this);
};

