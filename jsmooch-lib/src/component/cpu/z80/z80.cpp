//
// Created by Dave on 2/7/2024.
//

#include <cstring>
#include <cstdio>
#include <cassert>

#include "helpers/debugger/debuggerdefs.h"
#include "helpers/debugger/debugger.h"
#include "helpers/debug.h"
#include "z80.h"
#include "z80_opcodes.h"
#include "z80_disassembler.h"

namespace Z80 {

void regs::inc_R()
{
    R = (R & 0x80) | ((R + 1) & 0x7F);
}

void regs::exchange_shadow()
{
    BCt = (B << 8) | C;
    DEt = (D << 8) | E;
    HLt = (H << 8) | L;

    B = (BC_ & 0xFF00) >> 8;
    C = BC_ & 0xFF;
    D = (DE_ & 0xFF00) >> 8;
    E = DE_ & 0xFF;
    H = (HL_ & 0xFF00) >> 8;
    L = HL_ & 0xFF;

    BC_ = BCt;
    DE_ = DEt;
    HL_ = HLt;
}

void regs::exchange_de_hl()
{
    Ht = H;
    Lt = L;
    H = D;
    L = E;
    D = Ht;
    E = Lt;

}

void regs::exchange_shadow_af()
{
    AFt = (A << 8) | F.u;

    A = (AF_ & 0xFF00) >> 8;
    F.u = AF_ & 0xFF;

    AF_ = AFt;
}

core::core(bool in_CMOS) : CMOS(in_CMOS)
{
    dbg.events.IRQ = -1;
    dbg.events.NMI = -1;

    IRQ_pending = NMI_pending = NMI_ack = false;

    trace.last_cycle = 0;
    trace.cycles = &trace.my_cycles;
    trace.my_cycles = 0;
    current_instruction = nullptr;

    trace.ins_PC = 0;
}

void core::setup_tracing(jsm_debug_read_trace* dbg_read_trace, u64 *trace_cycle_pointer)
{
    jsm_copy_read_trace(&trace.strct, dbg_read_trace);
    trace.cycles = trace_cycle_pointer;
    trace.ok = 1;
}

static u32 prefix_to_codemap(u32 prefix) {
    switch (prefix) {
        case 0:
            return 0;
        case 0xCB:
            return MAX_OPCODE + 1;
        case 0xDD:
            return (MAX_OPCODE + 1) * 2;
        case 0xED:
            return (MAX_OPCODE + 1) * 3;
        case 0xFD:
            return (MAX_OPCODE + 1) * 4;
        case 0xDDCB:
            return (MAX_OPCODE + 1) * 5;
        case 0xFDCB:
            return (MAX_OPCODE + 1) * 6;
        default:
            printf("BAD PREFIX");
            return -1;
    }
}

ins_func core::fetch_decoded(u32 opcode, u32 prefix)
{
    return decoded_opcodes[prefix_to_codemap(prefix) + opcode];
}

void core::reset()
{
    regs.rprefix = regs::P_HL;
    regs.prefix = 0;
    regs.A = 0xFF;
    regs.F.u = 0xFF;
    regs.B = regs.C = regs.D = regs.E = 0;
    regs.H = regs.L = regs.IX = regs.IY = 0;
    regs.WZ = regs.PC = 0;
    regs.SP = 0xFFFF;
    regs.EI = regs.P = regs.Q = 0;
    regs.HALT = regs.IFF1 = regs.IFF2 = 0;
    regs.IM = 1;

    IRQ_pending = NMI_pending = NMI_ack = false;
    regs.IRQ_vec = -1;

    regs.IR = S_RESET;
    current_instruction = fetch_decoded(regs.IR, 0);
    regs.TCU = 0;
}

void core::notify_IRQ(bool level) {
    IRQ_pending = level;
}

void core::notify_NMI(bool level) {
    NMI_ack &= level;
    NMI_pending = NMI_pending || level;
}

void core::set_pins_opcode()
{
    pins.RD = pins.MRQ = pins.WR = pins.IO = 0;
    pins.Addr = regs.PC;
    regs.PC = (regs.PC + 1) & 0xFFFF;
}

void core::set_pins_nothing()
{
    pins.RD = pins.MRQ = 0;
    pins.WR = pins.IO = 0;
}

void core::set_instruction(u32 to)
{
    regs.IR = to;
    current_instruction = fetch_decoded(regs.IR, regs.prefix);
    regs.TCU = 0;
    regs.prefix = 0;
    regs.rprefix = regs::P_HL;
}

void core::ins_cycles()
{
    switch(regs.TCU) {
        // 1-4 is fetch next thing and interpret
        // addr T0-1
        // REFRESH on addr T2-3
        // MREQ on T1
        // RD on T1
        // data latch T2
        case 0: // already handled by fetch of next instruction starting
            set_pins_opcode();
            break;
        case 1: // T1 MREQ, RD
            if (regs.HALT) { regs.TCU = 0; break; }
            if (regs.poll_IRQ) {
                // Make sure we only do this at start of an instruction
                regs.poll_IRQ = false;
                if (NMI_pending && !NMI_ack) {
                    DBG_EVENT(dbg.events.NMI);
                    NMI_pending = false;
                    NMI_ack = true;
                    regs.IRQ_vec = 0x66;
                    pins.IRQ_maskable = false;
                    set_instruction(S_IRQ);
                    if (::dbg.brk_on_NMIRQ) {
                        dbg_break("Z80 NMI", *trace.cycles);
                    }
                } else if (IRQ_pending && regs.IFF1 && (!(regs.EI))) {
                    DBG_EVENT(dbg.events.IRQ);
                    regs.PC = (regs.PC - 1) & 0xFFFF;
                    pins.IRQ_maskable = true;
                    regs.IRQ_vec = 0x38;
                    pins.D = 0xFF;
                    set_instruction(S_IRQ);
                    if (::dbg.brk_on_NMIRQ) {
                        dbg_break("Z80 IRQ", *trace.cycles);
                    }
                }
            }
            pins.RD = 1;
            pins.MRQ = 1;
            pins.M1 = 1;
            break;
        case 2: // T2, RD to 0 and data latch, REFRESH and MRQ=1 for REFRESH
            if (pins.WAIT) {
                regs.TCU--;
                return;
            }
            pins.RD = 0;
            pins.MRQ = 0;
            pins.M1 = 0;
            regs.t[0] = pins.D;
            pins.Addr = (regs.I << 8) | regs.R;
            break;
        case 3: // T3 not much here
            // If we need to fetch another, start that and set TCU back to 1
            regs.inc_R();
            if (regs.t[0] == 0xDD) { regs.prefix = 0xDD; regs.rprefix = regs::P_IX; regs.TCU = -1; break; }
            if (regs.t[0] == 0xfD) { regs.prefix = 0xFD; regs.rprefix = regs::P_IY; regs.TCU = -1; break; }
            // elsewise figure out what to do next
            // this gets a little tricky
            // 4, 5, 6, 7, 8, 9, 10, 11, 12 = rprefix != HL and is CB, execute CBd
            if ((regs.t[0] == 0xCB) && (regs.rprefix != regs::P_HL)) {
                regs.prefix = ((regs.prefix << 8) | 0xCB) & 0xFFFF;
                break;
            }
                // . so 13, 14, 15, 16. opcode, then immediate execution CB
            else if (regs.t[0] == 0xCB) {
                regs.prefix = 0xCB;
                regs.TCU = 12;
                break;
            }
                // reuse 13-16
            else if (regs.t[0] == 0xED) {
                regs.prefix = 0xED;
                regs.TCU = 12;
                break;
            }
            else {
                //regs.prefix = 0x00;
                set_instruction(regs.t[0]);
                break;
            }
        case 4: // CBd begins here, as does operand()
            //
            switch(regs.rprefix) {
                case regs::P_HL:
                    regs.WZ = (regs.H << 8) | regs.L;
                    break;
                case regs::P_IX:
                    regs.WZ = regs.IX;
                    break;
                case regs::P_IY:
                    regs.WZ = regs.IY;
                    break;
            }
            set_pins_opcode();
            break;
        case 5: // operand() middle
            pins.RD = 1;
            pins.MRQ = 1;
            break;
        case 6: // operand() end
            regs.WZ = static_cast<u32>((static_cast<i32>(regs.WZ) + static_cast<i32>(static_cast<i8>(pins.D))) & 0xFFFF);
            set_pins_nothing();
            regs.TCU += 2;
            break;
        case 7: // wait a cycle
            break;
        case 8: // wait one more cycle
            break;
        case 9: // start opcode fetch
            set_pins_opcode();
            break;
        case 10:
            pins.RD = 1;
            pins.MRQ = 1;
            break;
        case 11: // cycle 3 of opcode tech
            set_pins_nothing();
            regs.t[0] = pins.D;
            set_instruction(regs.t[0]);
            break;
        case 12: // cycle 4 of opcode fetch. execute instruction!
            //set_instruction(regs.t[0]);
            break;
        case 13: // CB regular and ED regular starts here
            set_pins_opcode();
            break;
        case 14:
            pins.MRQ = 1;
            pins.RD = 1;
            break;
        case 15:
            pins.Addr = (regs.I << 8) | regs.R;
            regs.inc_R();
            regs.t[0] = pins.D;
            set_pins_nothing();
            break;
        case 16:
            // execute from CB or ED now
            set_instruction(regs.t[0]);
            break;
        default:
            assert(1!=0);
            break;
    }
}

void core::pprint_context(jsm_string &out) {
    out.sprintf("PC:%04X A:%02X B:%02X C:%02X D:%02X E:%02X H:%02X L:%02X SP:%04X IX:%04X IY:%04X I:%02X R:%02X WZ:%04X F:%02X TCU:%d" DBGC_RST,
        trace.ins_PC, regs.A, regs.B, regs.C, regs.D, regs.E,
        regs.H, regs.L, regs.SP, regs.IX, regs.IY, regs.I, regs.R,
        regs.WZ, regs.F.u, regs.TCU);
}


void core::printf_trace() {
    //char t[250];
    //t[0] = 0;
    u32 b = trace.strct.read_trace(trace.strct.ptr, trace.ins_PC);
    u32 mPC = trace.ins_PC;
    struct jsm_string out{250};
    disassemble(mPC, b, trace.strct, out);
    printf(DBGC_Z80 "\nZ80   (%06llu)     %04x  %s   ", *trace.cycles, trace.ins_PC, out.ptr);
    //dbg_seek_in_line(TRACE_BRK_POS);
    printf("PC:%04X A:%02X B:%02X C:%02X D:%02X E:%02X H:%02X L:%02X SP:%04X IX:%04X IY:%04X I:%02X R:%02X WZ:%04X F:%02X TCU:%d" DBGC_RST,
           trace.ins_PC, regs.A, regs.B, regs.C, regs.D, regs.E,
           regs.H, regs.L, regs.SP, regs.IX, regs.IY, regs.I, regs.R,
           regs.WZ, regs.F.u, regs.TCU);
}

void core::trace_format()
{
    if (trace.dbglog.view && trace.dbglog.view->ids_enabled[trace.dbglog.id]) {
        char t[250];
        t[0] = 0;
        trace.str.quickempty();
        trace.str2.quickempty();
        dbglog_view *dv = trace.dbglog.view;
        u64 tc;
        if (!trace.cycles) tc = 0;
        else tc = *trace.cycles;

        if (regs.IR > 255) {
            switch (regs.IR) {
                case Z80::S_IRQ:
                    trace.str.sprintf("IRQ");
                    break;
                case Z80::S_RESET:
                    trace.str.sprintf("RESET");
                    break;
                case Z80::S_DECODE:
                    trace.str.sprintf("\nDECODE? OOPS!");
                    break;
            }
            dv->add_printf(trace.dbglog.id, tc, DBGLS_TRACE, "%s", trace.str.ptr);
            dv->extra_printf("%s", trace.str2.ptr);
            return;
        }
        u32 ins_PC = trace.ins_PC;
        disassemble(ins_PC, regs.IR, trace.strct, trace.str);
        pprint_context(trace.str2);

        dv->add_printf(trace.dbglog.id, tc, DBGLS_TRACE, "%04x  %s", trace.ins_PC, trace.str.ptr);
        dv->extra_printf("%s", trace.str2.ptr);

        //Z80(   931)    008C  LDIR          TCU:1 PC:008E  A:00 B:1F C:F5 D:C0 E:0B H:C0 L:0A SP:DFF0 IX:0000 IY:0000 I:00 R:5E WZ:008D F:sZyhxPnc
        /*u32 b = trace.strct.read_trace(trace.strct.ptr, PCO);
        u32 mPC = PCO;
        disassemble(&mPC, b, &read_trace, t, sizeof(t));*/
        //dbg_printf(DBGC_Z80 "\nZ80   (%06llu)     %04x  %s", *trace.cycles, PCO, t);
        //dbg_seek_in_line(TRACE_BRK_POS);
        /*dbg_printf("PC:%04X A:%02X B:%02X C:%02X D:%02X E:%02X H:%02X L:%02X SP:%04X IX:%04X IY:%04X I:%02X R:%02X WZ:%04X F:%02X TCU:%d" DBGC_RST,
                   PCO, regs.A, regs.B, regs.C, regs.D, regs.E,
                   regs.H, regs.L, regs.SP, regs.IX, regs.IY, regs.I, regs.R,
                   regs.WZ, regs.F.u, regs.TCU);*/
    }

}

void core::lycoder_print()
{
    dbg_printf("\n%08d %04X %02X%02X %02X%02X %02X%02X %02X%02X %04X %04X", *trace.cycles, regs.PC, regs.A,
               regs.F.u, regs.B, regs.C, regs.D, regs.E,
               regs.H, regs.L, regs.IX, regs.IY);
}

void core::cycle()
{
    regs.TCU++;
    trace.my_cycles++;
#ifdef Z80_TRACE_BRK
    if (trace_cycles == Z80_TRACE_BRK) {
        printf("\nTRACE BREAK!");
        ::dbg.break();
    }
#endif
    if (regs.IR == S_DECODE) {
        // Long logic to decode opcodes and decide what to do
        if ((regs.TCU == 1) && (regs.prefix == 0)) trace.ins_PC = pins.Addr;
        ins_cycles();
    } else {
#ifdef Z80_DBG_SUPPORT
    if (regs.TCU == 1) {
        trace.last_cycle = trace.ins_PC;
        trace_format();
    }
#endif // Z80_DBG_SUPPORT
        // Execute an actual opcode
        current_instruction(regs, pins);
    }
}

u32 parity(u32 val) {
    val ^= val >> 4;
    val ^= val >> 2;
    val ^= val >> 1;
    return (val & 1) == 0;
}

#define S(x) Sadd(state, & x, sizeof( x))
void regs::serialize(serialized_state &state)
{
    S(IR);
    S(TCU);

    S(A);
    S(B);
    S(C);
    S(D);
    S(E);
    S(H);
    S(L);
    S(F.u);

    S(I);
    S(R);

    S(AF_);
    S(BC_);
    S(DE_);
    S(HL_);

    S(TR);
    S(TA);

    S(PC);
    S(SP);
    S(IX);
    S(IY);
    for (unsigned int & i : t) {
        S(i);
    }

    S(WZ);
    S(EI);
    S(P);
    S(Q);
    S(IFF1);
    S(IFF2);
    S(IM);
    S(HALT);
    S(data);
    S(IRQ_vec);
    S(rprefix);
    S(prefix);
    S(poll_IRQ);
}

void pins::serialize(serialized_state &state)
{
    S(Addr);
    S(D);
    S(IRQ_maskable);
    S(RD);
    S(WR);
    S(IO);
    S(MRQ);
    S(M1);
    S(WAIT);
}

void core::serialize(serialized_state &state)
{
    regs.serialize(state);
    pins.serialize(state);

    S(CMOS);
    S(IRQ_pending);
    S(NMI_pending);
    S(NMI_ack);

    S(trace.ins_PC);
}
#undef S

#define L(x) Sload(state, & x, sizeof( x))
void regs::deserialize(serialized_state &state)
{
    L(IR);
    L(TCU);

    L(A);
    L(B);
    L(C);
    L(D);
    L(E);
    L(H);
    L(L);
    L(F.u);

    L(I);
    L(R);

    L(AF_);
    L(BC_);
    L(DE_);
    L(HL_);

    L(TR);
    L(TA);

    L(PC);
    L(SP);
    L(IX);
    L(IY);
    for (unsigned int & i : t) {
        L(i);
    }

    L(WZ);
    L(EI);
    L(P);
    L(Q);
    L(IFF1);
    L(IFF2);
    L(IM);
    L(HALT);
    L(data);
    L(IRQ_vec);
    L(rprefix);
    L(prefix);
    L(poll_IRQ);
}

void pins::deserialize(serialized_state &state)
{
    L(Addr);
    L(D);
    L(IRQ_maskable);
    L(RD);
    L(WR);
    L(IO);
    L(MRQ);
    L(M1);
    L(WAIT);
}

void core::deserialize(serialized_state &state)
{
    regs.deserialize(state);
    pins.deserialize(state);

    L(CMOS);
    L(IRQ_pending);
    L(NMI_pending);
    L(NMI_ack);

    L(trace.ins_PC);
    current_instruction = fetch_decoded(regs.IR, regs.prefix);
}
#undef L

};