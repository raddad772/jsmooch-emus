//
// Created by Dave on 2/10/2024.
//

#include <cassert>
#include <cstdio>
#include "sh4_interpreter.h"
#include "sh4_interpreter_opcodes.h"
#include "fsca.h"
#include "tmu.h"
//#include "system/dreamcast/dreamcast.h"
#include "sh4_interrupts.h"

#define TIMER_IGNORE_DELAY_SLOT
//#define BRK 0x8c002774


// Endianness is little.

// disassembly printf args
#define SH_DISA_P_ARGS "\ncyc:%05d  %08x %s   ", *trace.cycles, regs.PC, disassembled[decoded_index][opcode]


#define CLOCK_DIVIDER 1
#define TIMER_MULTIPLIER (8)
namespace SH4 {

void REGS::SR_set(u32 val) {
    val &= 0x700083F3;
    u32 old_RB = SR.RB;

    SR.u = val;
    if(SR.MD) {
        if (old_RB != SR.RB) {
            swap_register_banks();
        }
    }
    else {
        if (SR.RB)
        {
            //printf("\nUpdateSR MD=0;RB=1 , this must noUpdateSR MD=0;RB=1 , this must not happen!t happen!"); // reicast says
            SR.RB = 0;//error - must always be 0 if not privileged
        }
        if (old_RB)
            swap_register_banks();
    }

    /*if (!regs.SR.MD) should_be = 0;
    else should_be = regs.SR.RB;
    if (should_be != regs.currently_banked_rb) {
#ifdef IRQ_DBG
        dbg_LT_printf(DBGC_RED "\nSWAP REGISTER BANKS cur:%d shouldbe:%d R0:%08x R0_BANKEd:%08x PC:%08X cyc:%llu" DBGC_RST, regs.currently_banked_rb, should_be, regs.R[0], regs.R_[0], regs.PC, clock.trace_cycles);
        printf(DBGC_RED "\nSWAP REGISTER BANKS cur:%d shouldbe:%d R0:%08x R0_BANKEd:%08x PC:%08X cyc:%llu" DBGC_RST, regs.currently_banked_rb, should_be, regs.R[0], regs.R_[0], regs.PC, clock.trace_cycles);
#endif
        regs.currently_banked_rb = should_be;
        swap_register_banks();
    }*/
}

u32 REGS::SR_get() {
    return SR.u;
}

void REGS::FPSCR_update(u32 old_RM, u32 old_DN)
{
    // 0=nearest, 1=zero, 2 & 3 = reserved
    /*assert(RM < 2);
    switch(RM) {
        case 0: // nearest
            fesetround(FE_TONEAREST);
            break;
        case 1: // to zero
            fesetround(FE_TOWARDZERO);
            break;
        case 2:
        case 3:
            printf("\nInvalid FPSCR RM %d", RM);
            break;
    }*/
    // Thanks to Reicast for this
/*    if ((old_RM!=RM) || (old_dn!=fpscr.DN))
    {
        old_rm=fpscr.RM ;
        old_dn=fpscr.DN ;*/

        //Correct rounding is required by some games (SOTB, etc)
#if BUILD_COMPILER == COMPILER_VC
        if (FPSCR.RM == 1)  //if round to 0 , set the flag
            _controlfp(_RC_CHOP, _MCW_RC);
        else
            _controlfp(_RC_NEAR, _MCW_RC);

        if (FPSCR.DN)     //denormals are considered 0
            _controlfp(_DN_FLUSH, _MCW_DN);
        else
            _controlfp(_DN_SAVE, _MCW_DN);
#else

        #if HOST_CPU==CPU_X86 || HOST_CPU==CPU_X64

            u32 temp=0x1f80;	//no flush to zero && round to nearest

			if (FPSCR.RM==1)  //if round to 0 , set the flag
				temp|=(3<<13);

			if (FPSCR.DN)     //denormals are considered 0
				temp|=(1<<15);
			asm("ldmxcsr %0" : : "m"(temp));
    #elif HOST_CPU==CPU_ARM
		static const unsigned int x = 0x04086060;
		unsigned int y = 0x02000000;
		if (FPSCR.RM==1)  //if round to 0 , set the flag
			y|=3<<22;

		if (FPSCR.DN)
			y|=1<<24;


		int raa;

		asm volatile
			(
				"fmrx   %0, fpscr   \n\t"
				"and    %0, %0, %1  \n\t"
				"orr    %0, %0, %2  \n\t"
				"fmxr   fpscr, %0   \n\t"
				: "=r"(raa)
				: "r"(x), "r"(y)
			);
	#elif HOST_CPU == CPU_ARM64
		static const unsigned long off_mask = 0x04080000;
        unsigned long on_mask = 0x02000000;    // DN=1 Any operation involving one or more NaNs returns the Default NaN

        if (FPSCR.RM == 1)		// if round to 0, set the flag
        	on_mask |= 3 << 22;

        if (FPSCR.DN)
        	on_mask |= 1 << 24;	// flush denormalized numbers to zero

        asm volatile
            (
                "MRS    x10, FPCR     \n\t"
                "AND    x10, x10, %0  \n\t"
                "ORR    x10, x10, %1  \n\t"
                "MSR    FPCR, x10     \n\t"
                :
                : "r"(off_mask), "r"(on_mask)
            );
    #else
		#if !defined(TARGET_EMSCRIPTEN)
			printf("SetFloatStatusReg: Unsupported platform\n");
		#endif
	#endif
#endif
    //}
}


u32 REGS::FPSCR_get() const
{
    return FPSCR.u;
}

void REGS::FPSCR_bankswitch()
{
    memcpy(&fb[2], &fb[0], 64);
    memcpy(&fb[0], &fb[1], 64);
    memcpy(&fb[1], &fb[2], 64);
}

void REGS::FPSCR_set(u32 val) {
    // If floating-point select register changed
    if (FPSCR.FR != ((val >> 21) & 1)) {
        FPSCR_bankswitch();
    }
    u32 old_RM = FPSCR.RM;
    u32 old_DN = FPSCR.DN;
    FPSCR.u = val;
    // Make sure correct FP rounding mode
    FPSCR_update(old_RM, old_DN);
}

void core::pprint(ins_t *ins, bool last_traces)
{
    u32 had_any = 0;
    i32 last_n = -1;
    i32 last_m = -1;
    if (!last_traces)
        dbg_seek_in_line(45);
    else
        dbg_LT_seek_in_line(45);
    if (ins->Rn != -1) {
        if (!last_traces)
            dbg_printf("R%d:%08x", ins->Rn, regs.R[ins->Rn]);
        else
            dbg_LT_printf("R%d:%08x", ins->Rn, regs.R[ins->Rn]);

        had_any = 1;
        last_n = static_cast<i32>(ins->Rn);
    }
    if (ins->Rm != -1) {
        if (had_any) {
            if (!last_traces)
                dbg_printf(" ");
            else
                dbg_LT_printf(" ");
        }
        had_any = 1;
        if (!last_traces)
            dbg_printf("R%d:%08x", ins->Rm, regs.R[ins->Rm]);
        else
            dbg_LT_printf("R%d:%08x", ins->Rm, regs.R[ins->Rm]);
        last_m = (i32)ins->Rm;
    }
    if ((pp_last_m != -1) && (pp_last_m != ins->Rm) && (pp_last_m != ins->Rn)) {
        if (had_any) {
            if (!last_traces)
                dbg_printf(" ");
            else
                dbg_LT_printf(" ");
        }
        had_any = 1;
        if (!last_traces)
            dbg_printf("R%d:%08x", pp_last_m, regs.R[pp_last_m]);
        else
            dbg_LT_printf("R%d:%08x", pp_last_m, regs.R[pp_last_m]);
    }
    if ((pp_last_n != -1) && (pp_last_n != ins->Rm) && (pp_last_n != ins->Rn)) {
        if (had_any) {
            if (!last_traces)
                dbg_printf(" ");
            else
                dbg_LT_printf(" ");
        }
        had_any = 1;
        if (!last_traces)
            dbg_printf("R%d:%08x", pp_last_n, regs.R[pp_last_n]);
        else
            dbg_LT_printf("R%d:%08x", pp_last_n, regs.R[pp_last_n]);
    }
    if ((ins->Rn != 0) && (ins->Rm != 0) && (pp_last_m != 0) && (pp_last_n != 0)) {
        if (had_any) {
            if (!last_traces)
                dbg_printf(" ");
            else
                dbg_LT_printf(" ");
        }
        if (!last_traces)
            dbg_printf(" R0:%08x", regs.R[0]);
        else
            dbg_LT_printf("R0:%08x", regs.R[0]);
    }

    if (!last_traces)
        dbg_printf(" IMASK:%x HIGHEST:%x T:%d", regs.SR.IMASK, interrupt_highest_priority, regs.SR.T);
    else
        dbg_LT_printf(" IMASK:%x HIGHEST:%x T:%d", regs.SR.IMASK, interrupt_highest_priority, regs.SR.T);


    pp_last_m = last_m;
    pp_last_n = last_n;
}

void core::lycoder_print(u32 opcode)
{
    if (dbg.trace_on)
        dbg_printf("\n%08x %04x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x",
                   regs.PC, opcode,
                   regs.R[0], regs.R[1], regs.R[2],
                   regs.R[3], regs.R[4], regs.R[5],
                   regs.R[6], regs.R[7], regs.R[8],
                   regs.R[9], regs.R[10], regs.R[11],
                   regs.R[12], regs.R[13], regs.R[14],
                   regs.R[15], regs.SR_get(), regs.FPSCR_get());
}

void core::fetch_and_exec(bool is_delay_slot)
{
    u32 opcode = fetch_ins(mptr, regs.PC);
#ifdef DBG_SUPPORT
#ifdef BRK
    if (regs.PC == BRK) {
        dbg_break();
    }
#endif // BRK
#endif

    *trace.cycles += CLOCK_DIVIDER;

#ifdef TIMER_IGNORE_DELAY_SLOT
    if (!is_delay_slot)
#endif
        clock.timer_cycles += TIMER_MULTIPLIER;

    cycles -= CLOCK_DIVIDER;
    ins_t *ins = &decoded[SH4_decoded_index][opcode];
#ifdef DO_LAST_TRACES
    dbg_LT_printf(SH_DISA_P_ARGS);
    pprint(ins, true);
    dbg_LT_endline();
#endif
#if defined(LYCODER) || defined(REICAST_DIFF)
    lycoder_print(opcode);
#else // !LYCODER and !REICAST_DIFF
#ifdef DBG_SUPPORT
    if (dbg.trace_on) {
        dbg_printf(SH_DISA_P_ARGS);
        pprint(ins, false);
    }
#endif // DBG_SUPPORT
#endif // else !LYCODER and !REICAST_DIFF

    (this->*ins->exec)(ins);
}

void core::run_cycles(u32 howmany) {
    // fetch
    cycles += static_cast<i32>(howmany);
    while(cycles > 0) {
        fetch_and_exec(false);
        if ((regs.SR.BL == 0) && (interrupt_highest_priority > regs.SR.IMASK)) {
#ifdef BRK_ON_NMIRQ
            dbg_break();
#endif
#ifdef IRQ_DBG
#define THING "\nINTERRUPT SERVICED AT %llu R0:%08x SR:%08x SSR:%08x IMASK:%d SB_ISTNRM:%08x SB_ISTEXT:%08x", clock.trace_cycles, regs.R[0], regs.SR_get(), regs.SSR, regs.SR.IMASK, dbg.dcptr->io.SB_ISTNRM.u, dbg.dcptr->io.SB_ISTEXT.u
            dbg_LT_printf(THING);
            dbg_printf(THING);
            printf(THING);
#undef THING
#endif
            interrupt();
        }
#ifdef DBG_SUPPORT
        if (dbg.do_break) {
            cycles = 0;
            break;
        }
#endif
    }
}

core::core(scheduler_t* scheduler_in) :
    tmu(this),
    scheduler(scheduler_in) {
    trace.my_cycles = 0;
    trace.ok = false;
    trace.cycles = &trace.my_cycles;
    clock.timer_cycles = 0;
    clock.trace_cycles_blocks = 0;
    pp_last_m = pp_last_n = -1;
    do_sh4_decode();
    init_interrupt_struct(interrupt_sources, interrupt_map);
    interrupt_highest_priority = 0;
    reset();
    //printf("\nINS! %s\n", disassembled[0][0x6772]);

    mptr = nullptr;
    fetch_ins = nullptr;
    read = nullptr;
    write = nullptr;
#ifndef TEST_SH4
    //tmu.reset();
#endif
}

void REGS::swap_register_banks()
{
    //printf(DBGC_RED "\nBANK SWAP RB:%d" DBGC_RST, SR.RB);
    for (u32 i = 0; i < 8; i++) {
        u32 t = R[i];
        R[i] = R_[i];
        R_[i] = t;
    }
}


void update_mode()
{
}

void core::reset()
{
    /* for SR,
     * MD bit = 1, RB bit = 1, BL bit = 1, FD bit = 0,
I3ñI0 = 1111 (H'F), reserved bits = 0, others
undefined
     */
    /*
     EXPEVT = H'00000000;
VBR = H'00000000;
SR.MD = 1;
SR.RB = 1;
SR.BL = 1;
SR.(I0-I3) = B'1111;
SR.FD=0;
Initialize_CPU();
Initialize_Module(PowerOn);
PC = H'A0000000;*
     */
    tmu.reset();
    regs.VBR = 0;
    regs.PC = 0xA0000000;
    regs.GBR = 0x8c000000;
    regs.FPSCR_set(0x00004001);
    regs.SR_set((regs.SR_get() &  0b11110011) | 0b01110000000000000000000011110000);
}

// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "helpers/multisize_memaccess.cpp"

u64 do_ma_read(void *ptr, u32 addr, u8 sz, bool* success) {
    auto *th = static_cast<core *>(ptr);
    return th->ma_read(addr, sz, success);
}

u64 core::ma_read(u32 addr, u8 sz, bool* success) {
    u32 full_addr = addr;
    u32 up_addr = addr | 0xE0000000;
    addr &= 0x1FFFFFFF;
    if ((full_addr < 0xE0000000) && (addr < 0x1C000000)) {
        printf("\nIllegal SH4 read forward! %08x", addr);
        *success = false;
        return 0;
    }

    if ((up_addr >= 0xFFD80000) && (up_addr <= 0xFFD8002C)) {
        return tmu.read(full_addr, sz, success);
    }

    // OC address array
    if ((up_addr >= 0xF4000000) && (up_addr <= 0xF4FFFFFF)) {
        return 0;
    }

    switch (addr | 0xE0000000) {
// NOLINTNEXTLINE(bugprone-suspicious-include)
        case 0xFF000030: // Undocumented CPU_VERSION
            return 0x040205c1;
        /*case 0xEC380A50: // unknown?
            dbg_break();
            return 0;
        case 0xEFFFFFFF: // unknown?
            return 0;*/
        case 0xFF800030: { // PDTRA
            assert(sz==2);
            // PDTRA from Bus Control
            // Note: I got it from Deecy...
            // Note: I have absolutely no idea what's going on here.
            //       This is directly taken from Flycast, kind already got it from Chankast.
            //       This is needed for the bios to work properly, without it, it will
            //       go to sleep mode with all interrupts disabled early on.
            u32 tpctra = regs.PCTRA;
            u32 tpdtra = regs.PDTRA;

            u16 tfinal = 0;
            if ((tpctra & 0xf) == 0x8) {
                tfinal = 3;
            } else if ((tpctra & 0xf) == 0xB) {
                tfinal = 3;
            } else {
                tfinal = 0;
            }

            if (((tpctra & 0xf) == 0xB) && ((tpdtra & 0xf) == 2)) {
                tfinal = 0;
            } else if (((tpctra & 0xf) == 0xC) && ((tpdtra & 0xf) == 2)) {
                tfinal = 3;
            }

            tfinal |= 0; // 0=VGA, 2=RGB, 3=composite //@intFromEnum(self._dc.?.cable_type) << 8;

            return 0x300 | tfinal;
        }
    }

    dbg_printf("\nMISSED SH4 READ %08x PC:%08x", full_addr, regs.PC);
    *success = false;
    return 0;
}

void core::set_QACR(u32 num, u32 val)
{
    regs.QACR[num] = val;
    u32 area = (val >> 2) & 7;
}

void core::console_add(u32 val, u8 sz)
{
    if (sz == 1) {
        if (val == '\n' || (console.cur - console.ptr) >= (console.allocated_len-1)) {
            printf("\n(CONSOLE) %s", console.ptr);
            //dbgloglog(trace.console_log_id, DBGLS_INFO, "%s", console.ptr);
            console.quickempty();
        }
        else {
            console.sprintf("%c", val);
        }
    }
}

void do_ma_write(void *ptr, u32 addr, u64 val, u8 sz, bool* success) {
    auto *th = static_cast<core *>(ptr);
    th->ma_write(addr, val, sz, success);
}

void core::ma_write(u32 addr, u64 val, u8 sz, bool* success) {
    // 1F000000-1FFFFFFF also mirrors this
    u32 full_addr = addr;
    u32 up_addr = addr | 0xF0000000;
    addr &= 0x1FFFFFFF;
    if ((full_addr < 0xE0000000) && (addr < 0x1C000000)) {
        printf("\nIllegal SH4 write forward! %08x", addr);
        *success = false;
        return;
    }

    if ((up_addr >= 0xFFD80000) && (up_addr <= 0xFFD8002C)) {
        tmu.write(full_addr, val, sz, success);
        return;
    }

    if ((full_addr >= 0xE0000000) && (full_addr <= 0xE3FFFFFF)) { // store queue write
        cW[sz](SQ[(addr >> 5) & 1], addr & 0x1C, val);
        return;
    }

    // OC address array
    if ((up_addr >= 0xF4000000) && (up_addr <= 0xF4FFFFFF)) {
        return;
    }

    switch(up_addr) {
        case 0x1fe8000c: {

        }
        case 0xFF000038: { set_QACR(0, val); return; }
        case 0xFF00003C: { set_QACR(1, val); return; }
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/regs_writes.cpp"
    }

    dbg_printf("\nMISSED SH4 WRITE %08x PC:%08x", full_addr, regs.PC);
    *success = false;
}

void core::give_memaccess(memaccess_t* to)
{
    to->ptr = (void *)this;
    to->read = &do_ma_read;
    to->write = &do_ma_write;
}

void core::setup_tracing(jsm_debug_read_trace *rt, u64 *trace_cycles_in)
{
    trace.ok = true;
    jsm_copy_read_trace(&read_trace, rt);
    trace.cycles = trace_cycles_in;
}

/* syscalls - https://mc.pp.se/dc/syscalls.html
 * Crazy Taxi
 * instructions - http://shared-ptr.com/sh_insns.html
 * originaldave_ — Today at 6:28 PM
how do you HLE the functions, is there documentation available?
[6:28 PM]
did you disassemble them and figure them out
[6:28 PM]
I kinda have this idea, that Dreamcast is, with the exception of a few emulators, not very well-documente

Senryoku — Today at 6:30 PM
Basically the bios loads two files "IP.bin" at 0x8000 and "1STREAD.BIN" (this one can have different names depending on the game) at 0x10000 and then jumps to, uuh, 0x8300 I think? I'd have to check that
[6:30 PM]
So I do that manually


 */
}