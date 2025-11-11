//
// Created by Dave on 2/10/2024.
//

#include <fenv.h>
#ifdef _MSC_VER
#pragma fenv_access (on)
#else
#pragma STDC FENV_ACCESS ON
#endif

#include "assert.h"
#include "stdio.h"
#include "string.h"
#include "stdarg.h"
#include "sh4_interpreter.h"
#include "sh4_interpreter_opcodes.h"
#include "fsca.h"
#include "tmu.h"
#include "system/dreamcast/dreamcast.h"
#include "sh4_interrupts.h"

#define SH4_TIMER_IGNORE_DELAY_SLOT
//#define SH4_BRK 0x8c002774


// Endianness is little.

// disassembly printf args
#define SH_DISA_P_ARGS "\ncyc:%05d  %08x %s   ", *this->trace.cycles, this->regs.PC, SH4_disassembled[SH4_decoded_index][opcode]


#define SH4_CLOCK_DIVIDER 1
#define SH4_TIMER_MULTIPLIER (8)

static void set_user_mode(struct SH4* this)
{

}

static void set_privileged_mode(struct SH4* this)
{

}

void SH4_regs_SR_reset(struct SH4_regs_SR* this)
{

}

u32 SH4_regs_SR_get(struct SH4_regs_SR* this)
{
    return (this->data & 0b10001111111111110111110000001100) | (this->MD << 30) | (this->RB << 29) | (this->BL << 28) | (this->FD << 15) |
            (this->M << 9) | (this->Q << 8) | (this->IMASK << 4) | (this->S << 1) | (this->T);
}

void SH4_regs_FPSCR_update(struct SH4_regs_FPSCR* this, u32 old_RM, u32 old_DN)
{
    // 0=nearest, 1=zero, 2 & 3 = reserved
    /*assert(this->RM < 2);
    switch(this->RM) {
        case 0: // nearest
            fesetround(FE_TONEAREST);
            break;
        case 1: // to zero
            fesetround(FE_TOWARDZERO);
            break;
        case 2:
        case 3:
            printf("\nInvalid FPSCR RM %d", this->RM);
            break;
    }*/
    // Thanks to Reicast for this
/*    if ((old_RM!=RM) || (old_dn!=fpscr.DN))
    {
        old_rm=fpscr.RM ;
        old_dn=fpscr.DN ;*/

        //Correct rounding is required by some games (SOTB, etc)
#if BUILD_COMPILER == COMPILER_VC
        if (this->RM == 1)  //if round to 0 , set the flag
            _controlfp(_RC_CHOP, _MCW_RC);
        else
            _controlfp(_RC_NEAR, _MCW_RC);

        if (this->DN)     //denormals are considered 0
            _controlfp(_DN_FLUSH, _MCW_DN);
        else
            _controlfp(_DN_SAVE, _MCW_DN);
#else

        #if HOST_CPU==CPU_X86 || HOST_CPU==CPU_X64

            u32 temp=0x1f80;	//no flush to zero && round to nearest

			if (this->RM==1)  //if round to 0 , set the flag
				temp|=(3<<13);

			if (this->DN)     //denormals are considered 0
				temp|=(1<<15);
			asm("ldmxcsr %0" : : "m"(temp));
    #elif HOST_CPU==CPU_ARM
		static const unsigned int x = 0x04086060;
		unsigned int y = 0x02000000;
		if (this->RM==1)  //if round to 0 , set the flag
			y|=3<<22;

		if (this->DN)
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

        if (this->RM == 1)		// if round to 0, set the flag
        	on_mask |= 3 << 22;

        if (this->DN)
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


u32 SH4_regs_FPSCR_get(struct SH4_regs_FPSCR* this)
{
    return (this->data & 0xFFC00000) | (this->FR << 21) | (this->SZ << 20) | (this->PR << 19) |
            (this->DN << 18) | (this->Cause << 12) | (this->Enable << 7) | (this->Flag << 2) | (this->RM);
}

void SH4_regs_FPSCR_bankswitch(struct SH4_regs* this)
{
    memcpy(&this->fb[2], &this->fb[0], 64);
    memcpy(&this->fb[0], &this->fb[1], 64);
    memcpy(&this->fb[1], &this->fb[2], 64);
}

void SH4_regs_FPSCR_set(struct SH4_regs* this, u32 val) {
    // If floating-point select register changed
    if (this->FPSCR.FR != ((val >> 21) & 1)) {
        SH4_regs_FPSCR_bankswitch(this);
    }
    u32 old_RM = this->FPSCR.RM;
    u32 old_DN = this->FPSCR.DN;
    this->FPSCR.data = val;
    this->FPSCR.FR = (val >> 21) & 1;
    this->FPSCR.SZ = (val >> 20) & 1;
    this->FPSCR.PR = (val >> 19) & 1;
    this->FPSCR.DN = (val >> 18) & 1;
    this->FPSCR.Cause = (val >> 12) & 63;
    this->FPSCR.Enable = (val >> 7) & 63;
    this->FPSCR.Flag = (val >> 2) & 63;
    this->FPSCR.RM = val & 3;

    // Make sure correct FP rounding mode
    SH4_regs_FPSCR_update(&this->FPSCR, old_RM, old_DN);
}

static void SH4_pprint(struct SH4* this, SH4_ins_t *ins, bool last_traces)
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
            dbg_printf("R%d:%08x", ins->Rn, this->regs.R[ins->Rn]);
        else
            dbg_LT_printf("R%d:%08x", ins->Rn, this->regs.R[ins->Rn]);

        had_any = 1;
        last_n = (i32)ins->Rn;
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
            dbg_printf("R%d:%08x", ins->Rm, this->regs.R[ins->Rm]);
        else
            dbg_LT_printf("R%d:%08x", ins->Rm, this->regs.R[ins->Rm]);
        last_m = (i32)ins->Rm;
    }
    if ((this->pp_last_m != -1) && (this->pp_last_m != ins->Rm) && (this->pp_last_m != ins->Rn)) {
        if (had_any) {
            if (!last_traces)
                dbg_printf(" ");
            else
                dbg_LT_printf(" ");
        }
        had_any = 1;
        if (!last_traces)
            dbg_printf("R%d:%08x", this->pp_last_m, this->regs.R[this->pp_last_m]);
        else
            dbg_LT_printf("R%d:%08x", this->pp_last_m, this->regs.R[this->pp_last_m]);
    }
    if ((this->pp_last_n != -1) && (this->pp_last_n != ins->Rm) && (this->pp_last_n != ins->Rn)) {
        if (had_any) {
            if (!last_traces)
                dbg_printf(" ");
            else
                dbg_LT_printf(" ");
        }
        had_any = 1;
        if (!last_traces)
            dbg_printf("R%d:%08x", this->pp_last_n, this->regs.R[this->pp_last_n]);
        else
            dbg_LT_printf("R%d:%08x", this->pp_last_n, this->regs.R[this->pp_last_n]);
    }
    if ((ins->Rn != 0) && (ins->Rm != 0) && (this->pp_last_m != 0) && (this->pp_last_n != 0)) {
        if (had_any) {
            if (!last_traces)
                dbg_printf(" ");
            else
                dbg_LT_printf(" ");
        }
        if (!last_traces)
            dbg_printf(" R0:%08x", this->regs.R[0]);
        else
            dbg_LT_printf("R0:%08x", this->regs.R[0]);
    }

    if (!last_traces)
        dbg_printf(" IMASK:%x HIGHEST:%x T:%d", this->regs.SR.IMASK, this->interrupt_highest_priority, this->regs.SR.T);
    else
        dbg_LT_printf(" IMASK:%x HIGHEST:%x T:%d", this->regs.SR.IMASK, this->interrupt_highest_priority, this->regs.SR.T);


    this->pp_last_m = last_m;
    this->pp_last_n = last_n;
}

void lycoder_print(struct SH4* this, u32 opcode)
{
    if (dbg.trace_on)
        dbg_printf("\n%08x %04x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x",
                   this->regs.PC, opcode,
                   this->regs.R[0], this->regs.R[1], this->regs.R[2],
                   this->regs.R[3], this->regs.R[4], this->regs.R[5],
                   this->regs.R[6], this->regs.R[7], this->regs.R[8],
                   this->regs.R[9], this->regs.R[10], this->regs.R[11],
                   this->regs.R[12], this->regs.R[13], this->regs.R[14],
                   this->regs.R[15], SH4_regs_SR_get(&this->regs.SR), SH4_regs_FPSCR_get(&this->regs.FPSCR));
}

void SH4_fetch_and_exec(struct SH4* this, u32 is_delay_slot)
{
    u32 opcode = this->fetch_ins(this->mptr, this->regs.PC);
#ifdef SH4_DBG_SUPPORT
#ifdef SH4_BRK
    if (this->regs.PC == SH4_BRK) {
        dbg_break();
    }
#endif // SH4_BRK
#endif

    *this->trace.cycles += SH4_CLOCK_DIVIDER;

#ifdef SH4_TIMER_IGNORE_DELAY_SLOT
    if (!is_delay_slot)
#endif
        this->clock.timer_cycles += SH4_TIMER_MULTIPLIER;

    this->cycles -= SH4_CLOCK_DIVIDER;
    struct SH4_ins_t *ins = &SH4_decoded[SH4_decoded_index][opcode];
#ifdef DO_LAST_TRACES
    dbg_LT_printf(SH_DISA_P_ARGS);
    SH4_pprint(this, ins, true);
    dbg_LT_endline();
#endif
#if defined(LYCODER) || defined(REICAST_DIFF)
    lycoder_print(this, opcode);
#else // !LYCODER and !REICAST_DIFF
#ifdef SH4_DBG_SUPPORT
    if (dbg.trace_on) {
        dbg_printf(SH_DISA_P_ARGS);
        SH4_pprint(this, ins, false);
    }
#endif // SH4_DBG_SUPPORT
#endif // else !LYCODER and !REICAST_DIFF

    ins->exec(this, ins);
}

void SH4_run_cycles(struct SH4* this, u32 howmany) {
    // fetch
    this->cycles += (i32)howmany;
    while(this->cycles > 0) {
        SH4_fetch_and_exec(this, 0);
        if ((this->regs.SR.BL == 0) && (this->interrupt_highest_priority > this->regs.SR.IMASK)) {
#ifdef BRK_ON_NMIRQ
            dbg_break();
#endif
#ifdef SH4_IRQ_DBG
#define THING "\nINTERRUPT SERVICED AT %llu R0:%08x SR:%08x SSR:%08x IMASK:%d SB_ISTNRM:%08x SB_ISTEXT:%08x", this->clock.trace_cycles, this->regs.R[0], SH4_regs_SR_get(&this->regs.SR), this->regs.SSR, this->regs.SR.IMASK, dbg.dcptr->io.SB_ISTNRM.u, dbg.dcptr->io.SB_ISTEXT.u
            dbg_LT_printf(THING);
            dbg_printf(THING);
            printf(THING);
#undef THING
#endif
            SH4_interrupt(this);
        }
#ifdef SH4_DBG_SUPPORT
        if (dbg.do_break) {
            this->cycles = 0;
            break;
        }
#endif
    }
}

void SH4_delete(struct SH4* this)
{
    jsm_string_delete(&this->console);
}

void SH4_init(struct SH4* this, scheduler_t* scheduler)
{
    this->trace.my_cycles = 0;
    this->trace.ok = 0;
    this->trace.cycles = &this->trace.my_cycles;
    this->clock.timer_cycles = 0;
    this->clock.trace_cycles_blocks = 0;
    this->scheduler = scheduler;
    this->pp_last_m = this->pp_last_n = -1;
    do_sh4_decode();
    jsm_string_init(&this->console, 5000);
    SH4_init_interrupt_struct(this->interrupt_sources, this->interrupt_map);
    this->interrupt_highest_priority = 0;
    SH4_reset(this);
    //printf("\nINS! %s\n", SH4_disassembled[0][0x6772]);

    this->mptr = NULL;
    this->fetch_ins = NULL;
    this->read = NULL;
    this->write = NULL;
#ifndef TEST_SH4
    TMU_init(this);
    TMU_reset(this);
#endif
}

static void swap_register_banks(struct SH4* this)
{
    //printf(DBGC_RED "\nBANK SWAP RB:%d" DBGC_RST, this->regs.SR.RB);
    for (u32 i = 0; i < 8; i++) {
        u32 t = this->regs.R[i];
        this->regs.R[i] = this->regs.R_[i];
        this->regs.R_[i] = t;
    }
}


void SH4_update_mode(struct SH4* this)
{
}

void SH4_SR_set(struct SH4* this, u32 val)
{
    // TODO: only do bank switch when MD is proper. DUH!
    val &= 0x700083F3;
    u32 old_RB = this->regs.SR.RB;

    this->regs.SR.data = val;
    this->regs.SR.MD = (val >> 30) & 1;
    this->regs.SR.RB = (val >> 29) & 1;
    this->regs.SR.BL = (val >> 28) & 1;
    this->regs.SR.FD = (val >> 15) & 1;
    this->regs.SR.M = (val >> 9) & 1;
    this->regs.SR.Q = (val >> 8) & 1;
    this->regs.SR.IMASK = (val >> 4) & 15;
    this->regs.SR.S = (val >> 1) & 1;
    this->regs.SR.T = val & 1;

    if(this->regs.SR.MD) {
        if (old_RB != this->regs.SR.RB) {
            swap_register_banks(this);
        }
    }
    else {
        if (this->regs.SR.RB)
        {
            //printf("\nUpdateSR MD=0;RB=1 , this must noUpdateSR MD=0;RB=1 , this must not happen!t happen!"); // reicast says
            this->regs.SR.RB = 0;//error - must always be 0 if not privileged
        }
        if (old_RB)
            swap_register_banks(this);
    }

    /*if (!this->regs.SR.MD) should_be = 0;
    else should_be = this->regs.SR.RB;
    if (should_be != this->regs.currently_banked_rb) {
#ifdef SH4_IRQ_DBG
        dbg_LT_printf(DBGC_RED "\nSWAP REGISTER BANKS cur:%d shouldbe:%d R0:%08x R0_BANKEd:%08x PC:%08X cyc:%llu" DBGC_RST, this->regs.currently_banked_rb, should_be, this->regs.R[0], this->regs.R_[0], this->regs.PC, this->clock.trace_cycles);
        printf(DBGC_RED "\nSWAP REGISTER BANKS cur:%d shouldbe:%d R0:%08x R0_BANKEd:%08x PC:%08X cyc:%llu" DBGC_RST, this->regs.currently_banked_rb, should_be, this->regs.R[0], this->regs.R_[0], this->regs.PC, this->clock.trace_cycles);
#endif
        this->regs.currently_banked_rb = should_be;
        swap_register_banks(this);
    }*/

}

void SH4_reset(struct SH4* this)
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
    this->regs.VBR = 0;
    this->regs.PC = 0xA0000000;
    this->regs.GBR = 0x8c000000;
    SH4_regs_FPSCR_set(&this->regs, 0x00004001);
    SH4_SR_set(this, (SH4_regs_SR_get(&this->regs.SR) &  0b11110011) | 0b01110000000000000000000011110000);
}

// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "helpers/multisize_memaccess.c"

u64 SH4_ma_read(void *ptr, u32 addr, u32 sz, u32* success)
{
    struct SH4* this = (struct SH4*)ptr;
    u32 full_addr = addr;
    u32 up_addr = addr | 0xE0000000;
    addr &= 0x1FFFFFFF;
    if ((full_addr < 0xE0000000) && (addr < 0x1C000000)) {
        printf("\nIllegal SH4 read forward! %08x", addr);
        *success = 0;
        return 0;
    }

    if ((up_addr >= 0xFFD80000) && (up_addr <= 0xFFD8002C)) {
        return TMU_read(this, full_addr, sz, success);
    }

    // OC address array
    if ((up_addr >= 0xF4000000) && (up_addr <= 0xF4FFFFFF)) {
        return 0;
    }

    switch (addr | 0xE0000000) {
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/regs_reads.c"
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
            u32 tpctra = this->regs.PCTRA;
            u32 tpdtra = this->regs.PDTRA;

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

    dbg_printf("\nMISSED SH4 READ %08x PC:%08x", full_addr, this->regs.PC);
    *success = 0;
    return 0;
}

static void set_QACR(struct SH4* this, u32 num, u32 val)
{
    this->regs.QACR[num] = val;
    u32 area = (val >> 2) & 7;
}

static void console_add(struct SH4* this, u32 val, u32 sz)
{
    if (sz == DC8) {
        jsm_string_sprintf(&this->console, "%c", val);
    }
}

void SH4_ma_write(void *ptr, u32 addr, u64 val, u32 sz, u32* success)
{
    // 1F000000-1FFFFFFF also mirrors this
    u32 full_addr = addr;
    u32 up_addr = addr | 0xF0000000;
    addr &= 0x1FFFFFFF;
    if ((full_addr < 0xE0000000) && (addr < 0x1C000000)) {
        printf("\nIllegal SH4 write forward! %08x", addr);
        *success = 0;
        return;
    }
    struct SH4* this = (struct SH4*)ptr;

    if ((up_addr >= 0xFFD80000) && (up_addr <= 0xFFD8002C)) {
        TMU_write(this, full_addr, val, sz, success);
        return;
    }

    if ((full_addr >= 0xE0000000) && (full_addr <= 0xE3FFFFFF)) { // store queue write
        cW[sz](this->SQ[(addr >> 5) & 1], addr & 0x1C, val);
        return;
    }

    // OC address array
    if ((up_addr >= 0xF4000000) && (up_addr <= 0xF4FFFFFF)) {
        return;
    }

    switch(up_addr) {
        case 0x1fe8000c: {

        }
        case 0xFF000038: { set_QACR(this, 0, val); return; }
        case 0xFF00003C: { set_QACR(this, 1, val); return; }
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "generated/regs_writes.c"
    }

    dbg_printf("\nMISSED SH4 WRITE %08x PC:%08x", full_addr, this->regs.PC);
    *success = 0;
}

void SH4_give_memaccess(struct SH4* this, SH4_memaccess_t* to)
{
    to->ptr = (void *)this;
    to->read = &SH4_ma_read;
    to->write = &SH4_ma_write;
}

void SH4_setup_tracing(struct SH4* this, jsm_debug_read_trace *rt, u64 *trace_cycles)
{
    this->trace.ok = 1;
    jsm_copy_read_trace(&this->read_trace, rt);
    this->trace.cycles = trace_cycles;
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