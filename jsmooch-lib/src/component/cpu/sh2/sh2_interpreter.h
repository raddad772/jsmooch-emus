//
// Created by . on 1/18/25.
//

#ifndef JSMOOCH_EMUS_SH2_INTERPRETER_H
#define JSMOOCH_EMUS_SH2_INTERPRETER_H

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/scheduler.h"

struct SH2_memaccess_t {
    void *ptr;
    u64 (*read)(void*,u32,u32,u32*);
    void (*write)(void*,u32,u64,u32,u32*);
};

struct SH2_regs_SR {
    u32 M, Q; //  9,8. Used in DIV instructions
    u32 IMASK; // 7-4. Interrupt mas bits
    u32 S; //       1. Saturation for MAC instruction
    u32 T; //       0. True/false or carry/borrow flag
    u32 data; // other bits
};

struct SH2_regs {
    u32 R[16]; // 16 general-purpose registers
    struct SH2_regs_SR SR; // Status Register
    u32 GBR; // Global Base Register
    u32 VBR; // Vector Base Register
    u32 MACH; // Multiply-Accumulate Hi
    u32 MACL; // Multiply-Accumulate Lo
    u32 PR; // Procedure Register
    u32 PC; // Program Counter
    
};

#define SH2I_NUM 23
enum SH2_interrupt_sources {
    sh2i_nmi = 0,
    sh2i_irl,
    sh2i_hitachi_udi,
    sh2i_gpio_gpioi,
    sh2i_dmac_dmte0,
    sh2i_dmac_dmte1,
    sh2i_dmac_dmte2,
    sh2i_dmac_dmte3,
    sh2i_dmac_dmae,
    sh2i_rtc_ati,
    sh2i_rtc_pri,
    sh2i_rtc_cui,
    sh2i_sci1_eri,
    sh2i_sci1_rxi,
    sh2i_sci1_txi,
    sh2i_sci1_tei,
    sh2i_scif_eri,
    sh2i_scif_rxi,
    sh2i_scif_bri,
    sh2i_scif_txi,
    sh2i_wdt_iti,
    sh2i_ref_rcmi,
    sh2i_ref_rovi
};

struct SH2_interrupt_source {
    enum SH2_interrupt_sources source;
    u32 priority;
    u32 sub_priority;
    u32 raised;
    u32 intevt;
};

struct SH2 {
    struct SH2_regs regs;

    // IRQ info
    struct SH2_interrupt_source interrupt_sources[SH2I_NUM];
    struct SH2_interrupt_source* interrupt_map[SH2I_NUM];
    u32 interrupt_highest_priority; // used to compare to IMASK

    // Memory & system access
    void *fetch_ptr;
    void *read_ptr;
    void *write_ptr;
    u32 (*fetch_ins)(void*,u32);
    u64 (*read)(void*,u32,u32);
    void (*write)(void*,u32,u64,u32);
    struct scheduler_t* scheduler;

    // Debug info
    struct {
        u32 ok;
        u64 *cycles;
        u64 my_cycles;
    } trace;
    // Debug things
    struct jsm_debug_read_trace read_trace;

};

void SH2_SR_set(SH2_regs_SR *, u32 val);
u32 SH2_SR_get(SH2_regs_SR *);

void SH2_init(SH2 *, scheduler_t *scheduler);
void SH2_reset(SH2 *);

#endif //JSMOOCH_EMUS_SH2_INTERPRETER_H
