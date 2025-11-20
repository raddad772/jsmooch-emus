//
// Created by . on 11/18/25.
//

#include <cassert>

#include "rca1802.h"
#include "rca1802_instructions.h"

namespace RCA1802 {

void core::reset() {
    regs.D = 0;
    regs.I = regs.N = pins.Q = 0;
    regs.IE = 1;
    pins.D = 0;
    regs.X = 0;
    regs.P = 0;
    regs.R[0].u = 0;
    prepare_fetch();
}

void core::dma_in() {
    pins.MRD = 0;
    pins.MWR = 1;
    pins.Addr = regs.R[0].u;
    regs.R[0].u++;
    pins.SC = pins::S2_dma;
}

void core::dma_out() {
    pins.MRD = 1;
    pins.MWR = 0;
    pins.Addr = regs.R[0].u;
    regs.R[0].u++;
    pins.SC = pins::S2_dma;
}

void core::dma_end() {
    prepare_fetch();
}

void core::interrupt_end() {
    prepare_fetch();
}

void core::interrupt() {
    regs.T.hi = regs.X;
    regs.T.lo = regs.P;
    regs.IE = 0;
    regs.P = 1;
    regs.X = 2;
    pins.SC = pins::S3_interrupt;
}

bool core::perform_interrupts() {
    // Sample DMA-IN, DMA-OUT, and IRQ vs. IE to determine fi any should be done, and do them, and return true.
    // False if no interrupt
    // priority DMA-IN, DMA-OUT, IRQ
    if (pins.DMA_IN) {
        dma_in();
        return true;
    }
    else if (pins.DMA_OUT) {
        dma_out();
        return true;
    }
    else if (pins.INTERRUPT) {
        interrupt();
        return true;
    }
    return false;
}

void core::prepare_fetch() {
    if constexpr (TEST_MODE) num_fetches++;
    if (!perform_interrupts()) { // Only proceed if no interrupt pending
        pins.SC = pins::S0_fetch;
        pins.Addr = regs.R[regs.P].u++;
        pins.MWR = 0;
        pins.MRD = 1;
    }
}


void core::do_load(u8 addr_ptr) {
    pins.MWR = 0;
    pins.MRD = 1;
    pins.Addr = regs.R[addr_ptr].u;
}

// prepare_fetch: prepare the fetch
// fetch: got it, so decode and prepare for execute

void core::do_out() {
    // M(R(X)) -> BUS
    // R(X) + 1 -> R(X)
    do_load(regs.X);
}

void core::do_store(u8 addr_ptr, u8 val) {
    pins.MWR = 1;
    pins.Addr = regs.R[addr_ptr].u;
    pins.D = val;
}

void core::prepare_execute() {
    pins.SC = pins::S1_execute;
    pins.MRD = pins.MWR = 0;
    execs_left = 1;
    switch (regs.I) {
        case 0x0: // LDN
            do_load(regs.N);
            ins = regs.N == 0 ? &ins_IDLE : &ins_LDN;
            break;
        case 0x1: // INC n
            ins = &ins_INC;
            break;
        case 0x2: // DEC n
            ins = &ins_DEC;
            break;
        case 0x3: // short branch
            ins = &ins_SHORT_BRANCH;
            do_immediate();
            break;
        case 0x4: // LDA
            ins = &ins_LDA;
            do_load(regs.N);
            break;
        case 0x5: // STR
            ins = &ins_none;
            do_store(regs.N, regs.D);
            break;
        case 0x6:
            if (regs.N == 0) {
                ins = &ins_IRX;
            }
            else {
                if (regs.N < 8) {
                    pins.N = regs.N;
                    do_out();
                    ins = &ins_OUT;
                }
                else if (regs.N == 8) {
                    ins = &ins_IRX;
                    printf("\n WARN 0x68;");
                }
                else {
                    pins.D = 0;
                    pins.N = regs.N & 7;
                    pins.MWR = 1;
                    pins.Addr = regs.R[regs.X].u;
                    ins = &ins_INP;
                }
            }
            break;
        case 0x7:
            prepare_execute_70();
            break;
        case 0x8: // GLO
            ins = &ins_GLO;
            break;
        case 0x9:
            ins = &ins_GHI;
            break;
        case 0xA:
            ins = &ins_PLO;
            break;
        case 0xB:
            ins = &ins_PHI;
            break;
        case 0xC: // long branch/skip
            ins = &ins_LONG_BRANCH;
            execs_left = 2;
            do_immediate();
            break;
        case 0xD: // N->P
            ins = &ins_SEP;
            break;
        case 0xE: // N->X
            ins = &ins_SEX;
            break;
        case 0xF:
            prepare_execute_F0();
            break;
    }
}

void core::prepare_execute_70() {
    switch (0x70 | regs.N) {
        case 0x70:
            ins = &ins_RET;
            do_load(regs.X);
            break;
        case 0x71:
            ins = &ins_DIS;
            do_load(regs.X);
            break;
        case 0x72: // LDXA
            ins = &ins_LDXA;
            do_load(regs.X);
            break;
        case 0x73: // STXD
            do_store(regs.X, regs.D);
            ins = &ins_STXD;
            break;
        case 0x74:
            ins = &ins_ADC;
            do_load(regs.X);
            break;
        case 0x75:
            ins = &ins_SDB;
            do_load(regs.X);
            break;
        case 0x76: // SHRC, RSHR
            ins = &ins_SHRC_RSHR;
            break;
        case 0x77: // SMB
            ins = &ins_SMB;
            do_load(regs.X);
            break;
        case 0x78: // SAV
            ins = &ins_none;
            do_store(regs.X, regs.T.u);
            break;
        case 0x79: // MARK
            regs.T.hi = regs.X;
            regs.T.lo = regs.P;
            do_store(2, regs.T.u);
            ins = &ins_MARK;
            break;
        case 0x7A:
            ins = &ins_REQ;
            break;
        case 0x7B:
            ins = &ins_SEQ;
            break;
        case 0x7C:
            ins = &ins_ADC;
            do_immediate();
            break;
        case 0x7D:
            ins = &ins_SDB;
            do_immediate();
            break;
        case 0x7E: // SHLC, RSHL
            ins = &ins_SHLC_RSHL;
            break;
        case 0x7F: // SMBI
            ins = &ins_SDB;
            do_immediate();
            break;
    }
}

void core::do_immediate() {
    do_load(regs.P);
    regs.R[regs.P].u++;
}

void core::prepare_execute_F0() {
    switch (0xF0 | regs.N) {
        case 0xF0:
            ins = &ins_LDN;
            do_load(regs.X);
            break;
        case 0xF1:
            ins = &ins_OR;
            do_load(regs.X);
            break;
        case 0xF2:
            ins = &ins_AND;
            do_load(regs.X);
            break;
        case 0xF3:
            ins = &ins_XOR;
            do_load(regs.X);
            break;
        case 0xF4:
            ins = &ins_ADD;
            do_load(regs.X);
            break;
        case 0xF5:
            ins = &ins_SD;
            do_load(regs.X);
            break;
        case 0xF6:
            ins = &ins_SHR;
            break;
        case 0xF7:
            ins = &ins_SM;
            do_load(regs.X);
            break;
        case 0xF8:
            ins = &ins_LDN;
            do_immediate();
            break;
        case 0xF9:
            ins = &ins_OR;
            do_immediate();
            break;
        case 0xFA:
            ins = &ins_AND;
            do_immediate();
            break;
        case 0xFB:
            ins = &ins_XOR;
            do_immediate();
            break;
        case 0xFC:
            ins = &ins_ADD;
            do_immediate();
            break;
        case 0xFD:
            ins = &ins_SD;
            do_immediate();
            break;
        case 0xFE:
            ins = &ins_SHL;
            break;
        case 0xFF:
            ins = &ins_SM;
            do_immediate();
            break;
    }
}

void core::fetch() {
    regs.IR = pins.D;
    regs.I = (regs.IR >> 4) & 15;
    regs.N = regs.IR & 15;
    prepare_execute();
}

void core::execute() {
    execs_left--;
    ins(this);
}

void core::cycle() {
    if (pins.clear_wait != pins::RUN) return;
    switch (pins.SC) {
        case pins::S0_fetch:
            fetch();
            break;
        case pins::S1_execute:
            execute();
            break;
        case pins::S2_dma:
            dma_end();
            break;
        case pins::S3_interrupt:
            interrupt_end();
            break;
    }
}

void core::setup_tracing(jsm_debug_read_trace *strct, u64 *trace_cycle_pointer)
{
    jsm_copy_read_trace(&trace.strct, strct);
    trace.ok = 1;
    trace.cycles = trace_cycle_pointer;
}

}