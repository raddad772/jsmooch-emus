//
// Created by . on 11/18/25.
//

#include "rca1802_instructions.h"
#include "rca1802.h"

namespace RCA1802 {

#define I(name) void ins_##name(core *core)

I(LDN) {
    core->regs.D = core->pins.D;
    core->prepare_fetch();
}

I(LDA) {
    core->regs.D = core->pins.D;
    core->regs.R[core->regs.N].u++;
    core->prepare_fetch();
}

I(INC) {
    core->regs.R[core->regs.N].u++;
    core->prepare_fetch();
}

I(DEC) {
    core->regs.R[core->regs.N].u--;
    core->prepare_fetch();
}

I(GLO) {
    core->regs.D = core->regs.R[core->regs.N].lo;
    core->prepare_fetch();
}

I(GHI) {
    core->regs.D = core->regs.R[core->regs.N].hi;
    core->prepare_fetch();
}

I(PLO) {
    core->regs.R[core->regs.N].lo = core->regs.D;
    core->prepare_fetch();
}

I(PHI) {
    core->regs.R[core->regs.N].hi = core->regs.D;
    core->prepare_fetch();
}

I(SEP) {
    core->regs.P = core->regs.N;
    core->prepare_fetch();
}

I(SEX) {
    core->regs.X = core->regs.N;
    core->prepare_fetch();
}

I(IRX) {
    core->regs.X = (core->regs.X + 1) & 15;
    core->prepare_fetch();
}

I(OUT) {
    core->pins.N = 0;
    core->regs.X = (core->regs.X + 1) & 15;
    core->prepare_fetch();
}

I(INP) {
    core->pins.N = 0;
    core->pins.MWR = 0;
    core->prepare_fetch();
}

I(LDXA) {
    core->regs.D = core->pins.D;
    core->regs.R[core->regs.X].u++;
    core->prepare_fetch();
}

I(STXD) {
    core->regs.R[core->regs.X].u--;
    core->prepare_fetch();
}

I(SHRC_RSHR) {
    u8 t = core->regs.DF << 7;
    core->regs.DF = core->regs.D & 1;
    core->regs.D = (core->regs.D >> 1) | t;
    core->prepare_fetch();
}

I(SHLC_RSHL) {
    u32 t = core->regs.DF;
    core->regs.DF = (core->regs.D >> 7) & 1;
    core->regs.D = ((core->regs.D << 1) | t) & 0xFF;
    core->prepare_fetch();
}

I(IDLE) {
    if (core->execs_left > 0) {
        if (!core->perform_interrupts()) core->execs_left += 2;
    }
    else {
        core->execs_left+=2;
    }

}

static u8 ADD(core *core, const u8 v1, const u8 v2) {
    const u16 out = static_cast<u16>(v1) + static_cast<u16>(v2);
    core->regs.DF = out > 0xFF;
    return static_cast<u8>(out);
}

static u8 ADC(core *core, const u8 v1, const u8 v2) {
    const u16 out = static_cast<u16>(v1) + static_cast<u16>(v2) + static_cast<u16>(core->regs.DF);
    core->regs.DF = out > 0xFF;
    return static_cast<u8>(out);
}

static u8 SUB(core *core, const u8 left, const u8 right) {
    const i16 l = left;
    const i16 r = right;
    const i16 out = l - r;
    core->regs.DF = out >= 0;
    return static_cast<u8>(out & 0xFF);
}

static u8 SBC(core *core, const u8 left, const u8 right) {
    const i16 l = left;
    const i16 r = right;
    const i16 df = static_cast<i16>(core->regs.DF ^ 1);
    const i16 out = l - r - df;
    core->regs.DF = out >= 0;
    return static_cast<u8>(out & 0xFF);
}

I(ADC) {
    core->regs.D = ADC(core, core->pins.D, core->regs.D);
    core->prepare_fetch();
}

I(SDB) {
    core->regs.D = SBC(core, core->pins.D, core->regs.D);
    core->prepare_fetch();
}

I(SMB) {
    core->regs.D = SBC(core, core->regs.D, core->pins.D);
    core->prepare_fetch();
}

I(SEQ) {
    core->pins.Q = 1;
    core->prepare_fetch();
}

I(REQ) {
    core->pins.Q = 0;
    core->prepare_fetch();
}

I(MARK) {
    core->regs.X = core->regs.P;
    core->regs.R[2].u--;
    core->prepare_fetch();
}


static void POP(core *core) {
    core->regs.R[core->regs.X].u++;
    core->regs.X = (core->pins.D >> 4) & 15;
    core->regs.P = core->pins.D & 15;
}

I(RET) {
    POP(core);
    core->regs.IE = 1;
    core->prepare_fetch();
}

I(DIS) {
    POP(core);
    core->regs.IE = 0;
    core->prepare_fetch();
}

I(OR) {
    core->regs.D |= core->pins.D;
    core->prepare_fetch();
}

I(AND) {
    core->regs.D &= core->pins.D;
    core->prepare_fetch();
}

I(XOR) {
    core->regs.D ^= core->pins.D;
    core->prepare_fetch();
}


I(ADD) {
    core->regs.D = ADD(core, core->regs.D, core->pins.D);
    core->prepare_fetch();
}

I(SD) {
    core->regs.D = SUB(core, core->pins.D, core->regs.D);
    core->prepare_fetch();
}

I(SM) {
    core->regs.D = SUB(core, core->regs.D, core->pins.D);
    core->prepare_fetch();
}

I(SHL) {
    core->regs.DF = (core->regs.D >> 7) & 1;
    core->regs.D <<= 1;
    core->prepare_fetch();
}

I(SHR) {
    core->regs.DF = core->regs.D & 1;
    core->regs.D >>= 1;
    core->prepare_fetch();
}

static bool do_short_branch(core *core) {
    bool do_branch = true;
    switch (core->regs.N & 7) {
#define K(num, X) case num: do_branch = X; break;
        K(0x0, true);
        K(0x1, core->pins.Q == 1);
        K(0x2, core->regs.D == 0);
        K(0x3, core->regs.DF == 1);
        K(0x4, (core->pins.EF & 1) == 1);
        K(0x5, (core->pins.EF & 2) == 2);
        K(0x6, (core->pins.EF & 4) == 4);
        K(0x7, (core->pins.EF & 8) == 8);
#undef K
    }
    if (core->regs.N & 8) do_branch = !do_branch;
    return do_branch;
}

I(SHORT_BRANCH) {
    if (do_short_branch(core)) core->regs.R[core->regs.P].lo = core->pins.D;
    core->prepare_fetch();
}

/*
 *SKips
 * C4 - NOP
 * C5
 * C6
 * C7

 * CC
 * CD
 * CE
 * CF
 *
 *LBranches
 * C0
 * C1
 * C2
 * C3
 * C8
 * C9
 * CA
 * CB
 *
 * if &4 then skip, else branch
 */

I(LONG_BRANCH) {
    if (core->execs_left == 1) {
        core->regs.B = core->pins.D;
        bool update_address = true;
        switch (core->regs.N) {
#define K(num, X) case num: update_address = X; break
            K(0x4, false);
            K(0x5, core->pins.Q == 0); // skip if Q==0, so update the address...
            K(0x6, core->regs.D != 0); // Skip if D!=0
            K(0x7, core->regs.DF == 0); // Skip if DF == 0
            K(0xC, core->regs.IE == 1);
            K(0xD, core->pins.Q == 1);
            K(0xE, core->regs.D == 0);
            K(0xF, core->regs.DF == 1);
#undef K
            default:
        }
        if (update_address) core->pins.Addr = core->regs.R[core->regs.P].u++;
    }
    else {
        bool condition = false;

        switch (core->regs.N) {
#define K(num, X) case num: condition = X; break
            K(0x0, true);
            K(0x1, core->pins.Q == 1);
            K(0x2, core->regs.D == 0);
            K(0x3, core->regs.DF == 1);
            K(0x8, false); // fail to branch
            K(0x9, core->pins.Q == 0);
            K(0xA, core->regs.D != 0);
            K(0xB, core->regs.DF == 0);

            K(0x4, false); // skip-never. so on false skip, PC-1
            K(0x5, core->pins.Q == 0); // skip if Q==0
            K(0x6, core->regs.D != 0); // Skip if D!=0
            K(0x7, core->regs.DF == 0); // Skip if DF == 0
            K(0xC, core->regs.IE == 1);
            K(0xD, core->pins.Q == 1);
            K(0xE, core->regs.D == 0);
            K(0xF, core->regs.DF == 1);
#undef K
        }
        bool is_skip = (core->regs.N & 4) >> 2;
        condition ^= is_skip;
        if (!is_skip && condition) {
            core->regs.R[core->regs.P].hi = core->regs.B;
            core->regs.R[core->regs.P].lo = core->pins.D;
        }
        else if (is_skip && condition) {
            core->regs.R[core->regs.P].u -= 1;
        }
        core->prepare_fetch();
    }
}

I(none) {
    core->prepare_fetch();
}

}