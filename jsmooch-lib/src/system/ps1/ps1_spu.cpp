//
// Created by . on 2/20/25.
//
#include <cassert>
#include "ps1_spu.h"

#include <ranges>

#include "ps1_bus.h"
#include "component/audio/ym2612/ym2612.h"
#include "helpers/multisize_memaccess.cpp"
#include "system/snes/snes_apu.h"

namespace PS1::SPU {

#define VOL(a,b) (static_cast<i16>(((static_cast<i32>(a) * static_cast<i32>(b)) >> 16)))

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

#define REVERB_REG(x) ((x - 0x1F801DC0) >> 1)

//  1F801DC0h rev00 dAPF1   disp    Reverb APF Offset 1
#define dAPF1 REVERB_REG(0x1F801DC0)
//  1F801DC2h rev01 dAPF2   disp    Reverb APF Offset 2
#define dAPF2 REVERB_REG(0x1F801DC2)
//  1F801DC4h rev02 vIIR    volume  Reverb Reflection Volume 1
#define vIIR REVERB_REG(0x1F801DC4)
//  1F801DC6h rev03 vCOMB1  volume  Reverb Comb Volume 1
#define vCOMB1 REVERB_REG(0x1F801DC6)
//  1F801DC8h rev04 vCOMB2  volume  Reverb Comb Volume 2
#define vCOMB2 REVERB_REG(0x1F801DC8)
//  1F801DCAh rev05 vCOMB3  volume  Reverb Comb Volume 3
#define vCOMB3 REVERB_REG(0x1F801DCA)
    //  1F801DCCh rev06 vCOMB4  volume  Reverb Comb Volume 4
#define vCOMB4 REVERB_REG(0x1F801DCC)
    //  1F801DCEh rev07 vWALL   volume  Reverb Reflection Volume 2
#define vWALL REVERB_REG(0x1F801DCE)
    //  1F801DD0h rev08 vAPF1   volume  Reverb APF Volume 1
#define vAPF1 REVERB_REG(0x1F801DD0)
    //  1F801DD2h rev09 vAPF2   volume  Reverb APF Volume 2
#define vAPF2 REVERB_REG(0x1F801DD2)
    //  1F801DD4h rev0A mLSAME  src/dst Reverb Same Side Reflection Address 1 Left
#define mLSAME REVERB_REG(0x1F801DD4)
    //  1F801DD6h rev0B mRSAME  src/dst Reverb Same Side Reflection Address 1 Right
#define mRSAME REVERB_REG(0x1F801DD6)
    //  1F801DD8h rev0C mLCOMB1 src     Reverb Comb Address 1 Left
#define mLCOMB1 REVERB_REG(0x1F801DD8)
    //  1F801DDAh rev0D mRCOMB1 src     Reverb Comb Address 1 Right
#define mRCOMB1 REVERB_REG(0x1F801DDA)
    //  1F801DDCh rev0E mLCOMB2 src     Reverb Comb Address 2 Left
#define mLCOMB2 REVERB_REG(0x1F801DDC)
    //  1F801DDEh rev0F mRCOMB2 src     Reverb Comb Address 2 Right
#define mRCOMB2 REVERB_REG(0x1F801DDE)
    //  1F801DE0h rev10 dLSAME  src     Reverb Same Side Reflection Address 2 Left
#define dLSAME REVERB_REG(0x1F801DE0)
    //  1F801DE2h rev11 dRSAME  src     Reverb Same Side Reflection Address 2 Right
#define dRSAME REVERB_REG(0x1F801DE2)
    //  1F801DE4h rev12 mLDIFF  src/dst Reverb Different Side Reflect Address 1 Left
#define mLDIFF REVERB_REG(0x1F801DE4)
    //  1F801DE6h rev13 mRDIFF  src/dst Reverb Different Side Reflect Address 1 Right
#define mRDIFF REVERB_REG(0x1F801DE6)
    //  1F801DE8h rev14 mLCOMB3 src     Reverb Comb Address 3 Left
#define mLCOMB3 REVERB_REG(0x1F801DE8)
    //  1F801DEAh rev15 mRCOMB3 src     Reverb Comb Address 3 Right
#define mRCOMB3 REVERB_REG(0x1F801DEA)
    //  1F801DECh rev16 mLCOMB4 src     Reverb Comb Address 4 Left
#define mLCOMB4 REVERB_REG(0x1F801DEC)
    //  1F801DEEh rev17 mRCOMB4 src     Reverb Comb Address 4 Right
#define mRCOMB4 REVERB_REG(0x1F801DEE)
    //  1F801DF0h rev18 dLDIFF  src     Reverb Different Side Reflect Address 2 Left
#define dLDIFF REVERB_REG(0x1F801DF0)
    //  1F801DF2h rev19 dRDIFF  src     Reverb Different Side Reflect Address 2 Right
#define dRDIFF REVERB_REG(0x1F801DF2)
    //  1F801DF4h rev1A mLAPF1  src/dst Reverb APF Address 1 Left
#define mLAPF1 REVERB_REG(0x1F801DF4)
    //  1F801DF6h rev1B mRAPF1  src/dst Reverb APF Address 1 Right
#define mRAPF1 REVERB_REG(0x1F801DF6)
    //  1F801DF8h rev1C mLAPF2  src/dst Reverb APF Address 2 Left
#define mLAPF2 REVERB_REG(0x1F801DF8)
    //  1F801DFAh rev1D mRAPF2  src/dst Reverb APF Address 2 Right
#define mRAPF2 REVERB_REG(0x1F801DFA)
// 1F801DFCh rev1E vLIN    volume  Reverb Input Volume Left
#define vLIN REVERB_REG(0x1F801DFC)
  // 1F801DFEh rev1F vRIN    volume  Reverb Input Volume Right
#define vRIN REVERB_REG(0x1F801DFE)

#define RR(x) io.reverb.regs[x]
#define RRA(x) (static_cast<u32>(io.reverb.regs[x]) << 3)

static constexpr i16 gauss_table[0x200] = {
    -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0002, 0x0002, 0x0002, 0x0003, 0x0003,
    0x0003, 0x0004, 0x0004, 0x0005, 0x0005, 0x0006, 0x0007, 0x0007,
    0x0008, 0x0009, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E,
    0x000F, 0x0010, 0x0011, 0x0012, 0x0013, 0x0015, 0x0016, 0x0018,
    0x0019, 0x001B, 0x001C, 0x001E, 0x0020, 0x0021, 0x0023, 0x0025,
    0x0027, 0x0029, 0x002C, 0x002E, 0x0030, 0x0033, 0x0035, 0x0038,
    0x003A, 0x003D, 0x0040, 0x0043, 0x0046, 0x0049, 0x004D, 0x0050,
    0x0054, 0x0057, 0x005B, 0x005F, 0x0063, 0x0067, 0x006B, 0x006F,
    0x0074, 0x0078, 0x007D, 0x0082, 0x0087, 0x008C, 0x0091, 0x0096,
    0x009C, 0x00A1, 0x00A7, 0x00AD, 0x00B3, 0x00BA, 0x00C0, 0x00C7,
    0x00CD, 0x00D4, 0x00DB, 0x00E3, 0x00EA, 0x00F2, 0x00FA, 0x0101,
    0x010A, 0x0112, 0x011B, 0x0123, 0x012C, 0x0135, 0x013F, 0x0148,
    0x0152, 0x015C, 0x0166, 0x0171, 0x017B, 0x0186, 0x0191, 0x019C,
    0x01A8, 0x01B4, 0x01C0, 0x01CC, 0x01D9, 0x01E5, 0x01F2, 0x0200,
    0x020D, 0x021B, 0x0229, 0x0237, 0x0246, 0x0255, 0x0264, 0x0273,
    0x0283, 0x0293, 0x02A3, 0x02B4, 0x02C4, 0x02D6, 0x02E7, 0x02F9,
    0x030B, 0x031D, 0x0330, 0x0343, 0x0356, 0x036A, 0x037E, 0x0392,
    0x03A7, 0x03BC, 0x03D1, 0x03E7, 0x03FC, 0x0413, 0x042A, 0x0441,
    0x0458, 0x0470, 0x0488, 0x04A0, 0x04B9, 0x04D2, 0x04EC, 0x0506,
    0x0520, 0x053B, 0x0556, 0x0572, 0x058E, 0x05AA, 0x05C7, 0x05E4,
    0x0601, 0x061F, 0x063E, 0x065C, 0x067C, 0x069B, 0x06BB, 0x06DC,
    0x06FD, 0x071E, 0x0740, 0x0762, 0x0784, 0x07A7, 0x07CB, 0x07EF,
    0x0813, 0x0838, 0x085D, 0x0883, 0x08A9, 0x08D0, 0x08F7, 0x091E,
    0x0946, 0x096F, 0x0998, 0x09C1, 0x09EB, 0x0A16, 0x0A40, 0x0A6C,
    0x0A98, 0x0AC4, 0x0AF1, 0x0B1E, 0x0B4C, 0x0B7A, 0x0BA9, 0x0BD8,
    0x0C07, 0x0C38, 0x0C68, 0x0C99, 0x0CCB, 0x0CFD, 0x0D30, 0x0D63,
    0x0D97, 0x0DCB, 0x0E00, 0x0E35, 0x0E6B, 0x0EA1, 0x0ED7, 0x0F0F,
    0x0F46, 0x0F7F, 0x0FB7, 0x0FF1, 0x102A, 0x1065, 0x109F, 0x10DB,
    0x1116, 0x1153, 0x118F, 0x11CD, 0x120B, 0x1249, 0x1288, 0x12C7,
    0x1307, 0x1347, 0x1388, 0x13C9, 0x140B, 0x144D, 0x1490, 0x14D4,
    0x1517, 0x155C, 0x15A0, 0x15E6, 0x162C, 0x1672, 0x16B9, 0x1700,
    0x1747, 0x1790, 0x17D8, 0x1821, 0x186B, 0x18B5, 0x1900, 0x194B,
    0x1996, 0x19E2, 0x1A2E, 0x1A7B, 0x1AC8, 0x1B16, 0x1B64, 0x1BB3,
    0x1C02, 0x1C51, 0x1CA1, 0x1CF1, 0x1D42, 0x1D93, 0x1DE5, 0x1E37,
    0x1E89, 0x1EDC, 0x1F2F, 0x1F82, 0x1FD6, 0x202A, 0x207F, 0x20D4,
    0x2129, 0x217F, 0x21D5, 0x222C, 0x2282, 0x22DA, 0x2331, 0x2389,
    0x23E1, 0x2439, 0x2492, 0x24EB, 0x2545, 0x259E, 0x25F8, 0x2653,
    0x26AD, 0x2708, 0x2763, 0x27BE, 0x281A, 0x2876, 0x28D2, 0x292E,
    0x298B, 0x29E7, 0x2A44, 0x2AA1, 0x2AFF, 0x2B5C, 0x2BBA, 0x2C18,
    0x2C76, 0x2CD4, 0x2D33, 0x2D91, 0x2DF0, 0x2E4F, 0x2EAE, 0x2F0D,
    0x2F6C, 0x2FCC, 0x302B, 0x308B, 0x30EA, 0x314A, 0x31AA, 0x3209,
    0x3269, 0x32C9, 0x3329, 0x3389, 0x33E9, 0x3449, 0x34A9, 0x3509,
    0x3569, 0x35C9, 0x3629, 0x3689, 0x36E8, 0x3748, 0x37A8, 0x3807,
    0x3867, 0x38C6, 0x3926, 0x3985, 0x39E4, 0x3A43, 0x3AA2, 0x3B00,
    0x3B5F, 0x3BBD, 0x3C1B, 0x3C79, 0x3CD7, 0x3D35, 0x3D92, 0x3DEF,
    0x3E4C, 0x3EA9, 0x3F05, 0x3F62, 0x3FBD, 0x4019, 0x4074, 0x40D0,
    0x412A, 0x4185, 0x41DF, 0x4239, 0x4292, 0x42EB, 0x4344, 0x439C,
    0x43F4, 0x444C, 0x44A3, 0x44FA, 0x4550, 0x45A6, 0x45FC, 0x4651,
    0x46A6, 0x46FA, 0x474E, 0x47A1, 0x47F4, 0x4846, 0x4898, 0x48E9,
    0x493A, 0x498A, 0x49D9, 0x4A29, 0x4A77, 0x4AC5, 0x4B13, 0x4B5F,
    0x4BAC, 0x4BF7, 0x4C42, 0x4C8D, 0x4CD7, 0x4D20, 0x4D68, 0x4DB0,
    0x4DF7, 0x4E3E, 0x4E84, 0x4EC9, 0x4F0E, 0x4F52, 0x4F95, 0x4FD7,
    0x5019, 0x505A, 0x509A, 0x50DA, 0x5118, 0x5156, 0x5194, 0x51D0,
    0x520C, 0x5247, 0x5281, 0x52BA, 0x52F3, 0x532A, 0x5361, 0x5397,
    0x53CC, 0x5401, 0x5434, 0x5467, 0x5499, 0x54CA, 0x54FA, 0x5529,
    0x5558, 0x5585, 0x55B2, 0x55DE, 0x5609, 0x5632, 0x565B, 0x5684,
    0x56AB, 0x56D1, 0x56F6, 0x571B, 0x573E, 0x5761, 0x5782, 0x57A3,
    0x57C3, 0x57E2, 0x57FF, 0x581C, 0x5838, 0x5853, 0x586D, 0x5886,
    0x589E, 0x58B5, 0x58CB, 0x58E0, 0x58F4, 0x5907, 0x5919, 0x592A,
    0x593A, 0x5949, 0x5958, 0x5965, 0x5971, 0x597C, 0x5986, 0x598F,
    0x5997, 0x599E, 0x59A4, 0x59A9, 0x59AD, 0x59B0, 0x59B2, 0x59B3
};

void VOICE::adpcm_start() {
    adpcm.cur_addr = adpcm.start_addr;
    pitch_counter = 0;
    adpcm_decode();
}

void VOICE::adpcm_decode() {
    u8 data[16];
    u32 start_addr = adpcm.cur_addr;
    for (u32 i = 0; i < 16; i++) {
        data[i] = bus->spu.read_RAM8(adpcm.cur_addr, true , true);
        adpcm.cur_addr++;
    }
    u8 flag = data[1];
    if (flag & 4) { // Loop start
        adpcm.repeat_addr = start_addr;
    }
    if (flag & 1) { // loop end
        io.reached_loop_end = 1;
        adpcm.cur_addr = adpcm.repeat_addr;
        if (!(flag & 2)) { // loop repeat
            env.phase = EP_RELEASE;
            env.adsr.output = 0;
            env.load_release();
        }
    }
    const u8 hd = data[0];
    u8 shift = hd & 0xF;
    if (shift > 13) shift = 9;
    shift = 12 - shift;
    u8 filter = (hd >> 4) & 7;
    if (filter > 4) filter = 4; // TODO: ??

    u32 idx = 2;
    u32 nibble = 0;
    // +0 = current
    // +1 = oldest
    // +2 = older
    // +3 = old
    i32 older = gauss.samples[(gauss.idx + 2) & 3];
    i32 old = gauss.samples[(gauss.idx + 3) & 3];
    for (short & s_out : adpcm.samples) {
        i32 t;
        if (nibble == 0) {
            t = data[idx] & 0x0F;
        }
        else {
            t = data[idx] >> 4;
            idx++;
        }
        nibble ^= 1;
        // t is a signed 4-bit value so convert to 32-bit signed
        t = (t & 7) | (t & 8 ? 0xFFFFFFF8 : 0);
        i32 s = (t << shift);
        i32 filtered;
        switch (filter) {
            case 0:
                filtered = s;
                break;
            case 1:
                filtered = s + (60 * old + 32) / 64;
                break;
            case 2:
                filtered = s + (115 * old - 52 * older + 32) / 64;
                break;
            case 3:
                filtered = s + (98 * old - 55 * older + 32) / 64;
                break;
            case 4:
                filtered = s + (122 * old - 60 * older + 32) / 64;
                break;
            default:
                NOGOHERE;
        }

        if (filtered < -0x8000) s = -0x8000;
        if (filtered > 0x7FFF) s = 0x7FFF;
        s_out = filtered;
        older = old;
        old = filtered;
    }
}

void FIFO::push(u16 item) {
    if (len >= 32) {
        printf("\n(SPU) FIFO OVERFLOW!");
        return;
    }
    items[tail] = item;
    tail = (tail + 1) & 31;
    len++;
}

u16 FIFO::pop() {
    if (len < 1) {
        printf("\n(SPU) FIFO UNDERFLOW!");
        return 0xFFFF;
    }
    u16 v = items[head];
    head = (head + 1) & 31;
    len--;
    return v;
}

void FIFO::clear() {
    len = head = tail = 0;
}

void core::update_IRQs() {
    u32 old = io.IRQ_level;
    io.IRQ_level = io.SPUCNT.irq9_enable && io.SPUCNT.master_enable && io.SPUSTAT.irq9;
    if (old != io.IRQ_level) bus->set_irq(IRQ_SPU, io.IRQ_level);
}

void ADSR_ENVELOPE::load_attack() {
    adsr.exponential = attack_exponential;
    adsr.decreasing = 0;
    adsr.shift = attack_shift;
    adsr.step = attack_step;
    adsr.output = 0;
    phase = EP_ATTACK;
    adsr.calc();
}

void ADSR_ENVELOPE::load_decay() {
    adsr.exponential = 1;
    adsr.decreasing = 1;
    adsr.shift = decay_shift;
    adsr.step = 0;
    adsr.output = 0x7FFF;
    phase = EP_DECAY;
    adsr.calc();
}

void ADSR_ENVELOPE::load_sustain() {
    adsr.exponential = sustain_exponential;
    adsr.decreasing = sustain_decreasing;
    adsr.shift = sustain_shift;
    adsr.step = sustain_step;
    adsr.output = sustain_level;
    phase = EP_SUSTAIN;
    adsr.calc();
}

void ADSR_ENVELOPE::load_release() {
    adsr.exponential = release_exponential;
    adsr.decreasing = 1;
    adsr.shift = release_shift;
    adsr.step = 0;
    phase = EP_RELEASE;
    adsr.calc();
}


void ADSR_ENVELOPE::cycle() {
    adsr.cycle();
    switch (phase) {
        case EP_ATTACK:
            if (adsr.output >= 0x7FFF) {
                load_decay();
            }
            break;
        case EP_DECAY:
            if (adsr.output <= sustain_level) {
                load_sustain();
            }
            break;
        case EP_SUSTAIN:
        case EP_RELEASE:
            break;
        default:
            NOGOHERE;
    }
}

void ADSR_GENERATOR::cycle() {
    counter--;
    if (counter > 0) return;
    counter = counter_reload;
    //if ((counter & 0x8000) == 0) return;

    output += adsr_step;
    if (!decreasing) {
        if (output < -0x8000) output = -0x8000;
        if (output > 0x7FFF) output = 0x7FFF;
    }
    else if (negative) {
        if (output < -0x8000) output = -0x8000;
        if (output > 0) output = 0;
    }
    else {
        output = MAX(output, 0);
    }
}

void VOICE_VOL::cycle() {
    // Adavnce the voice
    sweep.cycle();
}

static constexpr i32 dirtable[2][4] = {
    { 7, 6, 5, 4}, // 0=increase
    {-8, -7, -6, -5} // 1=decrease
};

void ADSR_GENERATOR::calc() {
    // ; Precalculation, can be cached on phase begin.
    // AdsrStep = 7 - StepValue
    // IF Decreasing XOR PhaseNegative THEN
    //  AdsrStep = NOT AdsrStep ; +7,+6,+5,+4 => -8,-7,-6,-5
    adsr_step = dirtable[decreasing][step];

    // AdsrStep = AdsrStep SHL Max(0,11-ShiftValue)
    i32 r = 11 - shift;
    r = MAX(0, r);
    adsr_step <<= r;

    // CounterIncrement = 8000h SHR Max(0,ShiftValue-11)
    r = shift - 11;
    r = MAX(0, r);
    counter_reload = 1 << r;

    // IF exponential AND increase AND AdsrLevel>6000h THEN
    if (exponential && !decreasing && output>0x6000) {
        //   IF ShiftValue < 10 THEN
        if (shift < 10)
        //     AdsrStep /= 4 ; SHR 2
            adsr_step >>= 2;
        //   ELSE IF ShiftValue >= 11 THEN
        else if (shift >= 11)
        //     CounterIncrement /= 4 ; SHR 2
            counter_reload <<= 2;
        else {
            //   ELSE
            //     AdsrStep /= 4 ; SHR 2
            //     CounterIncrement /= 4 ; SHR 2
            adsr_step >>= 2;
            counter_reload <<= 2;
        }
    }
    // ELSE IF exponential AND decrease THEN
    else if (exponential && decreasing) {
        //   AdsrStep=AdsrStep*AdsrLevel/8000h
        adsr_step = (adsr_step * output) >> 15;
    }
    // IF (StepValue | (ShiftValue SHL 2)) != ALL_BITS THEN
    r = (step | (shift << 2));
    if (r != 0b1111111) {
        //   CounterIncrement = MAX(CounterIncrement, 1)
        counter_reload = MIN(counter_reload, 0x8000);
    }
    counter = counter_reload;
}

void VOICE_VOL::write(u16 v) {
    io_val = v;
    mode = static_cast<VV_MODE>((v >> 15) & 1);
    if (mode == VVM_FIXED) sweep.output = static_cast<i16>(v << 1);
    else {
        // Do sweep stuff!
        sweep.exponential = (v >> 14) & 1;
        sweep.decreasing = (v >> 13) & 1;
        phase = (v >> 12) & 1;
        sweep.shift = (v >> 2) & 0x1F;
        sweep.step = v & 3;
        sweep.output = sweep.decreasing ? 0x7FFF : 0;
        sweep.calc();
        sweep.counter = sweep.counter_reload;
    }
}

u16 VOICE_VOL::read() {
    return io_val;
}

void VOICE::adpcm_get_sample() {
    gauss.idx = (gauss.idx + 1) & 3;
    gauss.samples[gauss.idx] = adpcm.samples[pitch_counter >> 12];
}

void VOICE::gaussian_me_up() {
    u16 gauss_index = (pitch_counter >> 4) & 0xFF;
    u32 idx = (gauss.idx + 1) & 3; // oldest
    i32 v = (gauss_table[0xFF-gauss_index] * gauss.samples[idx]) >> 15;
    idx = (idx + 1) & 3; // older
    v += (gauss_table[0x1FF-gauss_index] * gauss.samples[idx]) >> 15;
    idx = (idx + 1) & 3; // old
    v += (gauss_table[0x100+gauss_index] * gauss.samples[idx]) >> 15;
    v += (gauss_table[gauss_index] * gauss.samples[idx]) >> 15;
    if (v < -0x8000) v = -0x8000;
    if (v > 0x7FFF) v = 0x7FFF;
    sample = v;
}

void VOICE::cycle(i16 noise_level) {
    i32 step = io.sample_rate;
    if (io.PMON && num > 0) {
        i32 factor = bus->spu.voices[num-1].sample;
        factor += 0x8000;
        factor = static_cast<i32>(static_cast<i16>(factor));
        i32 estep = step;
        estep = (estep * factor) >> 15;
        step = estep & 0xFFFF;
    }

    // TODO: sample every 768 samples
    if (step > 0x3FFF) step = 0x4000;
    env.cycle();
    if (io.vol_l.mode) io.vol_l.cycle();
    if (io.vol_r.mode) io.vol_r.cycle();

    pitch_counter = (pitch_counter + step);
    if (pitch_counter >= 0x1C000) {
        pitch_counter -= 0x1C000;
        adpcm_decode();
    }
    adpcm_get_sample();
    gaussian_me_up();
    if (io.noise_enable) sample = VOL(noise_level, env.adsr.output);
    else sample = VOL(sample, env.adsr.output);
    sample_l = VOL(io.vol_l.sweep.output, sample);
    sample_r = VOL(io.vol_r.sweep.output, sample);
}

static void sch_FIFO_transfer(void *ptr, u64 key, u64 timecode, u32 jitter) {
    auto *th = static_cast<SPU::core *>(ptr);
    th->FIFO_transfer(timecode - jitter);
}

void core::DMA_write(u32 val) {
    if (!io.SPUSTAT.data_transfer_dma_write_req) {
        printf("\n(SPU) WARN DMA write ignored wrong mode %d", io.SPUCNT.sound_ram_transfer_mode);
        return;
    }
    latch.RAM_transfer_addr &= 0x7FFFF;
    write_RAM(latch.RAM_transfer_addr, val & 0xFFFF, true);
    latch.RAM_transfer_addr = (latch.RAM_transfer_addr + 2) & 0x7FFFF;
    write_RAM(latch.RAM_transfer_addr, val >> 16, true);
    latch.RAM_transfer_addr += 2;
}

u32 core::DMA_read() {
    if (!io.SPUSTAT.data_transfer_dma_read_req) {
        printf("\n(SPU) WARN DMA read ignored wrong mode %d", io.SPUCNT.sound_ram_transfer_mode);
        return 0xFFFFFFFF;
    }

    latch.RAM_transfer_addr &= 0x7FFFF;
    u32 v = read_RAM(latch.RAM_transfer_addr, true, true);
    latch.RAM_transfer_addr = (latch.RAM_transfer_addr + 2) & 0x7FFFF;
    v |= read_RAM(latch.RAM_transfer_addr, true, true) << 16;
    latch.RAM_transfer_addr += 2;
    return v;
}

void core::commit_FIFO() {
    while (FIFO.len > 0) {
        //if (io.RAMCNT.mode != 1) printf("\n(SPU) WARN MANUAL FIFO WRITE MODE!=2!");
        latch.RAM_transfer_addr &= 0x7FFFF;
        u16 v = FIFO.pop();
        write_RAM(latch.RAM_transfer_addr, v, true);
        latch.RAM_transfer_addr += 2;
    }
    io.SPUSTAT.data_transfer_busy = 0;
}
void core::FIFO_instant_transfer(u16 val) {
    latch.RAM_transfer_addr &= 0x7FFFF;
    write_RAM(latch.RAM_transfer_addr, val, true);
    latch.RAM_transfer_addr += 2;
}

void core::FIFO_transfer(u64 clock) {
    if (FIFO.len > 0) {
        //if (io.RAMCNT.mode != 1) printf("\n(SPU) WARN MANUAL FIFO WRITE MODE!=2!");
        latch.RAM_transfer_addr &= 0x7FFFF;
        u16 v = FIFO.pop();
        write_RAM(latch.RAM_transfer_addr, v, true);
        latch.RAM_transfer_addr += 2;
    }
    if (FIFO.len != 0) {
        schedule_FIFO_transfer(clock);
    }
    else {
        io.SPUSTAT.data_transfer_busy = 0;
    }
}

void core::schedule_FIFO_transfer(u64 clock) {
    // i64 timecode, u64 key, void *ptr, scheduler_callback callback, u32 *still_sched
    FIFO.sch_id = bus->scheduler.only_add_abs(clock + 60, 0, this, &sch_FIFO_transfer, &FIFO.still_sch);
}

static u32 constexpr masksz[5] = {0, 0xFF, 0xFFFF, 0, 0xFFFFFFFF };

void core::write_control_regs(u32 addr, u8 sz, u32 val) {
    switch (addr) {
        case 0x1F801DA4: // IRQ9 addr
            io.IRQ_addr = val << 3;
            return;
        case 0x1F801DA6: // RAM transfer address
            io.RAM_transfer_addr = val;
            latch.RAM_transfer_addr = val << 3;
            return;
        case 0x1F801DA8: // FIFO write
            //FIFO_instant_transfer(val);
            FIFO.push(val);
            //if (io.SPUCNT.sound_ram_transfer_mode == 1) commit_FIFO();
            //FIFO.push(val);
            //printf("\nFIFO write len:%d", FIFO.len);
            // if we're in data transfer mode, schedule it if it's not
            /*if (io.SPUCNT.sound_ram_transfer_mode == 1 && !FIFO.still_sch) {
                printf("\n(FIFO) drained reschedule");
                schedule_FIFO_transfer(bus->clock.master_cycle_count);
                io.SPUSTAT.data_transfer_busy = 1;
            }*/
            return;
        case 0x1F801DAA: // SPUCNT
            write_SPUCNT(val);
            return;
        case 0x1F801DAC: // RAMCNT
            io.RAMCNT.u = val;
            return;
        case 0x1F801DAE: // SPUSTAT
            return;
        case 0x1F801DB0:
            io.vol_CD_L = val;
            return;
        case 0x1F801DB2:
            io.vol_CD_R = val;
            return;
        case 0x1F801DB4:
            io.vol_ext_L = val;
            return;
        case 0x1F801DB6:
            io.vol_ext_R = val;
            return;
        case 0x1F801DB8:
            printf("\n(SPU) WARN SET CUR VOL L");
            io.cur_vol_L = val;
            return;
        case 0x1F801DBA:
            printf("\n(SPU) WARN SET CUR VOL R");
            io.cur_vol_R = val;
            return;
    }
    printf("\n(SPU) Unhandled CTRL write %08x(%d): %08x", addr, sz, val);
}

void core::write_SPUCNT(u16 val) {
    io.SPUCNT.u = val & 0xFFFF;

    io.SPUSTAT.u = (io.SPUSTAT.u & ~0b111111) | (val & 0b111111);
    io.SPUSTAT.data_transfer_dma_rw_req = (val >> 5) & 1;
    io.SPUSTAT.data_transfer_dma_read_req = 0;
    io.SPUSTAT.data_transfer_dma_write_req = 0;
    /*if (io.SPUCNT.sound_ram_transfer_mode != 1 && FIFO.still_sch) {
        printf("\n(SPU) warn FIFO commit cancel!");
        bus->scheduler.delete_if_exist(FIFO.sch_id);
        io.SPUSTAT.data_transfer_busy = 0;
    }*/
    //printf("\n(SPU) Transfer mode %d", io.SPUCNT.sound_ram_transfer_mode);
    switch (io.SPUCNT.sound_ram_transfer_mode) {
        case 0: // OFF
            break;
        case 1: // Manual Write/commit FIFO
            /*if (FIFO.len > 0) {
                printf("\n(SPU) Already has %d FIFO items!", FIFO.len);
                schedule_FIFO_transfer(bus->clock.master_cycle_count);
                io.SPUSTAT.data_transfer_busy = 1;
            }*/
            commit_FIFO();
            break;
        case 2: // DMAWrite
            io.SPUSTAT.data_transfer_dma_write_req = 1;
            break;
        case 3: // DMARead
            io.SPUSTAT.data_transfer_dma_read_req = 1;
            break;
    }
    if (!io.SPUCNT.irq9_enable) io.SPUSTAT.irq9 = 0;

    noise.step = io.SPUCNT.noise_frequency_step + 4;
    noise.shift = io.SPUCNT.noise_frequency_shift;
    update_IRQs();
}

u32 core::read_control_regs(u32 addr, u8 sz) {
    switch (addr) {
        case 0x1F801DA4: // IRQ9 addr
            return io.IRQ_addr << 3;
        case 0x1F801DA6:
            return io.RAM_transfer_addr;
        case 0x1F801DA8:
            printf("\n(SPU) FIFO READ WHAT?!");
            return 0xFFFF;
        case 0x1F801DAA: // SPUCNT
            return io.SPUCNT.u;
        case 0x1F801DAC: // RAMCNT
            return io.RAMCNT.u;
        case 0x1F801DAE: // SPUSTAT
            return io.SPUSTAT.u;
        case 0x1F801DB0:
            return io.vol_CD_L;
        case 0x1F801DB2:
            return io.vol_CD_R;
        case 0x1F801DB4:
            return io.vol_ext_L;
        case 0x1F801DB6:
            return io.vol_ext_R;
        case 0x1F801DB8:
            return io.cur_vol_L;
        case 0x1F801DBA:
            return io.cur_vol_R;
    }
    printf("\n(SPU) Unhandled CTRL read %08x(%d)", addr, sz);
    return 0xFFFF;
}

void core::write_reverb_reg(u32 addr, u8 sz, u32 val) {
    addr -= 0x1F801DC0;
    addr >>= 1;
    // TODO: real regs
    io.reverb.regs[addr] = val;

}

u32 core::read_reverb_reg(u32 addr, u8 sz) {
    addr -= 0x1F801DC0;
    addr >>= 1;
    // TODO: real regs
    return io.reverb.regs[addr];
}


void core::do_capture() {
    u32 addr_add = io.SPUSTAT.capture_buffer_half ? 0x200 : 0;
    i16 l, r;
    bus->cdrom.get_CD_audio(l, r);
    write_RAM(addr_add + capture.index, l, true);
    write_RAM(0x400 + addr_add + capture.index, r, true);
    write_RAM(0x800 + addr_add + capture.index, voices[1].sample, true);
    write_RAM(0xC00 + addr_add + capture.index, voices[3].sample, true);
    addr_add ^= 0x200;
    capture.sample.cd_l = static_cast<i16>(read_RAM(addr_add+capture.index, true, true));
    capture.sample.cd_r = static_cast<i16>(read_RAM(0x400+addr_add+capture.index, true, true));
    capture.sample.v1 = static_cast<i16>(read_RAM(0x800+addr_add+capture.index, true, true));
    capture.sample.v3 = static_cast<i16>(read_RAM(0xC00+addr_add+capture.index, true, true));

    capture.index = (capture.index + 2) & 0x1FF;
    if (capture.index == 0) {
#ifndef LYCODER
        io.SPUSTAT.capture_buffer_half ^= 1;
#endif
    }
}

void core::do_noise() {
    noise.timer -= noise.step;
    u32 bit = (noise.level >> 15) & 1;
    bit ^= (noise.level >> 12) & 1;
    bit ^= (noise.level >> 11) & 1;
    bit ^= (noise.level >> 10) & 1;
    bit ^= 1;
    if (noise.timer < 0) {
        noise.level = (noise.level << 1) + bit;
        noise.timer += (20000 >> noise.shift);
    }
    if (noise.timer < 0) {
        noise.timer += (20000 >> noise.shift);
    }

}

static constexpr i32 reverb_FIR[39] = {
    -0x0001,  0x0000,  0x0002,  0x0000, -0x000A,  0x0000,  0x0023, 0x0000,
-0x0067,  0x0000,  0x010A,  0x0000, -0x0268,  0x0000,  0x0534,  0x0000,
-0x0B90,  0x0000,  0x2806,  0x4000,  0x2806,  0x0000, -0x0B90,  0x0000,
 0x0534,  0x0000, -0x0268,  0x0000,  0x010A,  0x0000, -0x0067,  0x0000,
 0x0023,  0x0000, -0x000A,  0x0000,  0x0002,  0x0000, -0x0001,
};

void FIR_filter::add_sample(i32 smp) {
    samples[pos] = smp;
    pos = (pos + 1) % 39;
}

i32 FIR_filter::run() {
    i32 o = 0;
    u32 p = pos;
    for (const i32 s : reverb_FIR) {
        o += (samples[p] * s) >> 15;
        p = (p + 1) % 39;
    }
    return o;
}

void core::process_reverb() {
    // Ingest L/R samples
    reverb.filter_l.in.add_sample(reverb.in_l);
    reverb.filter_r.in.add_sample(reverb.in_r);


    if (!reverb.counter) { // 22050 clock
        // Actually run reverb now...
        i32 l = (reverb.filter_l.in.run() * static_cast<i32>(static_cast<i16>(RR(vLIN)))) >> 15;
        i32 r = (reverb.filter_r.in.run() * static_cast<i32>(static_cast<i16>(RR(vRIN)))) >> 15;

        apply_reflection(l, r);
        apply_comb_filter();
        apply_all_pass_filter_1();
        apply_all_pass_filter_2();
        reverb.proc_l *= static_cast<i32>(static_cast<i16>(RR(io.reverb.vol_out_l)));
        reverb.proc_r *= static_cast<i32>(static_cast<i16>(RR(io.reverb.vol_out_r)));
        reverb.proc_l >>= 15;
        reverb.proc_r >>= 15;
        if (reverb.proc_l < -0x8000) reverb.proc_l = -0x8000;
        if (reverb.proc_l > 0x7FFF) reverb.proc_l = 0x7FFF;
        if (reverb.proc_r < -0x8000) reverb.proc_r = -0x8000;
        if (reverb.proc_r > 0x7FFF) reverb.proc_r = 0x7FFF;
        reverb.filter_l.out.add_sample(reverb.proc_l);
        reverb.filter_r.out.add_sample(reverb.proc_r);
        reverb.buf_addr = reverb_addr(reverb.buf_addr + 2);
    }
    else { // cycle 1, add a 0
        reverb.filter_l.out.add_sample(0);
        reverb.filter_r.out.add_sample(0);
    }

    // Run the sample filter to get our 44kHz output
    reverb.sample_l = reverb.filter_l.out.run() << 1;
    reverb.sample_r = reverb.filter_r.out.run() << 1;

    reverb.counter ^= 1;
}
#define RVOL(a,b) ((static_cast<i32>(a)*static_cast<i32>(b))>>15)

void core::apply_all_pass_filter_1() {
    // ___Late Reverb APF1 (All Pass Filter 1, with input from COMB)________________
    // Lout=Lout-vAPF1*[mLAPF1-dAPF1]
    // Rout=Rout-vAPF1*[mRAPF1-dAPF1]
    u32 mal = reverb_addr(RRA(mLAPF1) - RRA(dAPF1));
    reverb.proc_l -= RVOL(RR(vAPF1), RAM[mal]);

    // [mLAPF1]=Lout
    RAM[reverb_addr(RRA(mLAPF1))] = reverb.proc_l;

    // Lout=Lout*vAPF1+[mLAPF1-dAPF1]
    reverb.proc_l = RVOL(reverb.proc_l,RR(vAPF1)) + RAM[mal];

    // Rout=Rout-vAPF1*[mRAPF1-dAPF1]
    u32 mar = reverb_addr(RRA(mRAPF1) - RRA(dAPF1));
    reverb.proc_r -= RVOL(RR(vAPF1), RAM[mar]);

    // [mRAPF1]=Rout
    RAM[reverb_addr(RRA(mRAPF1))] = reverb.proc_r;

    // Rout=Rout*vAPF1+[mRAPF1-dAPF1]
    reverb.proc_r = RVOL(reverb.proc_r, RR(vAPF1)) + RAM[mar];
}

void core::apply_all_pass_filter_2() {
    //___Late Reverb APF2 (All Pass Filter 2, with input from APF1)________________
    // Lout=Lout-vAPF2*[mLAPF2-dAPF2], [mLAPF2]=Lout, Lout=Lout*vAPF2+[mLAPF2-dAPF2]
    // Rout=Rout-vAPF2*[mRAPF2-dAPF2], [mRAPF2]=Rout, Rout=Rout*vAPF2+[mRAPF2-dAPF2]
    u32 mal = reverb_addr(RRA(mLAPF2) - RRA(dAPF2));
    reverb.proc_l -= RVOL(RR(vAPF2), RAM[mal]);
    RAM[reverb_addr(RRA(mLAPF2))] = reverb.proc_l;
    reverb.proc_l = RVOL(reverb.proc_l, RR(vAPF2)) + RAM[mal];

    u32 mar = reverb_addr(RRA(mRAPF2) - RRA(dAPF2));
    reverb.proc_r -= RVOL(RR(vAPF2), RAM[mar]);
    RAM[reverb_addr(RRA(mRAPF2))] = reverb.proc_r;
    reverb.proc_r = RVOL(reverb.proc_r, RR(vAPF2)) + RAM[mar];
}

void core::do_reflect(i32 smp, u32 d, u32 m) {
    //am[m_addr] = (input_sample + ram[d_addr] * vWALL - ram[m_addr - 2]) * vIIR + ram[m_addr - 2]
    i32 d_addr = reverb_addr(d);
    i32 m_addr = reverb_addr(m);
    i32 mm2 = m_addr - 2;
    if (mm2 < 0) mm2 = 0x7FFFE;
    i32 s = smp + RVOL(RAM[d_addr],RR(vWALL)) - RAM[mm2];
    s = VOL(s,RR(vIIR));
    s += RAM[mm2];
    RAM[m_addr] = s;
}

void core::apply_comb_filter() {
    /*
    __Early Echo (Comb Filter, with input from buffer)__________________________
    Lout=vCOMB1*[mLCOMB1]+vCOMB2*[mLCOMB2]+vCOMB3*[mLCOMB3]+vCOMB4*[mLCOMB4]
    Rout=vCOMB1*[mRCOMB1]+vCOMB2*[mRCOMB2]+vCOMB3*[mRCOMB3]+vCOMB4*[mRCOMB4]}
    */
    u32 ml1 = reverb_addr(RR(mLCOMB1));
    u32 ml2 = reverb_addr(RR(mLCOMB2));
    u32 ml3 = reverb_addr(RR(mLCOMB3));
    u32 ml4 = reverb_addr(RR(mLCOMB4));
    u32 mr1 = reverb_addr(RR(mRCOMB1));
    u32 mr2 = reverb_addr(RR(mRCOMB2));
    u32 mr3 = reverb_addr(RR(mRCOMB3));
    u32 mr4 = reverb_addr(RR(mRCOMB4));
    //     Lout=vCOMB1*[mLCOMB1]+vCOMB2*[mLCOMB2]+vCOMB3*[mLCOMB3]+vCOMB4*[mLCOMB4]

    reverb.proc_l = RVOL(RR(vCOMB1), RAM[ml1]);
    reverb.proc_l += RVOL(RR(vCOMB2), RAM[ml2]);
    reverb.proc_l += RVOL(RR(vCOMB3), RAM[ml3]);
    reverb.proc_l += RVOL(RR(vCOMB4), RAM[ml4]);
    reverb.proc_r = RVOL(RR(vCOMB1), RAM[mr1]);
    reverb.proc_r += RVOL(RR(vCOMB2), RAM[mr2]);
    reverb.proc_r += RVOL(RR(vCOMB3), RAM[mr3]);
    reverb.proc_r += RVOL(RR(vCOMB4), RAM[mr4]);
}

void core::apply_reflection(i32 l, i32 r) {
    do_reflect(l, RRA(dLSAME), RRA(mLSAME));
    do_reflect(r, RRA(dRSAME), RRA(mRSAME));
    do_reflect(l, RRA(dLDIFF), RRA(mLDIFF));
    do_reflect(r, RRA(dRDIFF), RRA(mRDIFF));
}

void core::cycle() {
    do_noise();
    reverb.in_l = 0;
    reverb.in_r = 0;
    for (auto & v : voices) {
        v.cycle(noise.level);
    }
    do_capture();

    local_clock++;

    i32 l = 0;
    i32 r = 0;
    for (u32 i = 0; i < 24; i++) {
        l += voices[i].sample_l;
        r += voices[i].sample_r;
        if (io.SPUCNT.reverb_master_enable && voices[i].io.reverb_on) {
            reverb.in_l += voices[i].sample_l;
            reverb.in_r += voices[i].sample_r;
        }

    }
    if (io.SPUCNT.cd_audio_reverb) {
        reverb.in_l += capture.sample.cd_l;
        reverb.in_r += capture.sample.cd_r;
    }
    if (reverb.in_l < -0x8000) reverb.in_l = -0x8000;
    if (reverb.in_l > 0x7FFF) reverb.in_l = 0x7FFF;
    if (reverb.in_r < -0x8000) reverb.in_r = -0x8000;
    if (reverb.in_r > 0x7FFF) reverb.in_r = 0x7FFF;
    process_reverb();

    //l += capture.sample.cd_l;
    //r += capture.sample.cd_r;
    //l += reverb.sample_l;
    //r += reverb.sample_r;
    l = reverb.sample_l;
    r = reverb.sample_r;

    // TODO: add cdrom audio to mix
    if (l < -0x8000) l = -0x8000;
    if (l > 0x7FFF) l = 0x7FFF;

    if (r < -0x8000) r = -0x8000;
    if (r > 0x7FFF) r = 0x7FFF;

    // TODO: test DMA IRQs
    sample_l = static_cast<i16>(l);
    sample_r = static_cast<i16>(r);
}

void core::mainbus_write(u32 addr, u8 sz, u32 val)
{
    u32 raddr = addr;
    //if (raddr != 0x1f801da8) dbg_printf("\n(SPU) Write %08x(%d): %08x", addr, sz, val);
    if ((sz == 1) && (addr & 1)) {
        static int a = 1;
        if (a == 1) {
            a = 0;
            printf("\n(SPU) WARN Ignore SPU 8bit write to odd address");
        }
        return;
    }
    addr &= 0xFFFFFFFE;
    if (sz == 4) {
        mainbus_write(addr, 2, val & 0xFFFF);
        mainbus_write(addr + 2, 2, (val >> 16) & 0xFFFF);
        return;
    }
    if (sz == 1) sz = 2;
    val &= 0xFFFF;

    if (addr >= 0x1F801C00 && addr <= 0x1F801D7F) {
        addr -= 0x1F801C00;
        addr %= 0x180;
        return voices[addr >> 4].write_reg((addr & 0xF) >> 1, val);
    }

    if (addr >= 0x1F801DA4 && addr <= 0x1F801DBB) {
        return write_control_regs(addr, sz, val);
    }
    if (addr >= 0x1F801DC0 && addr <= 0x1F801DFF) {
        return write_reverb_reg(addr, sz, val);
    }

    if ((addr >= 0x1F801E00) && (addr <= 0x1F801E5F)) {
        // 00 = l
        // 02 = r
        addr -= 0x1F801E00;
        u32 v = addr >> 2;
        if (addr & 2) voices[v].io.vol_r.sweep.output = static_cast<i16>(val);
        else voices[v].io.vol_l.sweep.output = static_cast<i16>(val);
        return;
    }

    switch (addr) {
        case 0x1F801D90:
            for (u32 i = 1; i < 16; i++) {
                voices[i].io.PMON = (val >> i) & 1;
            }
            io.pmon_lo = val;
            return;
        case 0x1F801D92:
            for (u32 i = 0; i < 8; i++) {
                voices[i+16].io.PMON = (val >> i) & 1;
            }
            io.pmon_hi = val;
            return;
        case 0x1F801D80: io.vol_L = val; return;
        case 0x1F801D82: io.vol_R = val; return;
        case 0x1F801D88:
            for (u16 i = 0; i < 16; i++) {
                if ((val >> i) & 1) voices[i].keyon();
            }
            io.keyon_lo = val;
            return;
        case 0x1F801D8A:
            for (u16 i = 0; i < 8; i++) {
                if ((val >> i) & 1) voices[i+16].keyon();
            }
            io.keyon_hi = val;
            return;
        case 0x1F801D8C:
            //printf("\nKEYOFF_LO %08x: %04x", addr, val);
            for (u16 i = 0; i < 16; i++) {
                if ((val >> i) & 1) voices[i].keyoff();
            }
            io.keyoff_lo = val;
            return;
        case 0x1F801D8E:
            //printf("\nKEYOFF_HI %08x: %04x", addr, val);
            for (u16 i = 0; i < 8; i++) {
                if ((val >> i) & 1) voices[i+16].keyoff();
            }
            io.keyoff_hi = val;
            return;
        case 0x1F801D94:
            for (u16 i = 0; i < 16; i++) {
                voices[i].io.noise_enable = (val >> i) & 1;
            }
            io.non_lo = val;
            return;
        case 0x1F801D96:
            for (u16 i = 0; i < 8; i++) {
                voices[i+16].io.noise_enable = (val >> i) & 1;
            }
            io.non_hi = val;
            return;
        case 0x1F801D98:
            for (u16 i = 0; i < 16; i++) {
                voices[i].io.reverb_on = (val >> i) & 1;
            }
            io.reverb.on_lo = val;
            return;
        case 0x1F801D9A:
            for (u16 i = 0; i < 8; i++) {
                voices[i+16].io.reverb_on = (val >> i) & 1;
            }
            io.reverb.on_hi = val;
            return;
        case 0x1F801D84: io.reverb.vol_out_l = val; return;
        case 0x1F801D86: io.reverb.vol_out_r = val; return;
        case 0x1F801DA2: io.reverb.mBASE = val << 3; return;
    }
    printf("\n(SPU) Unhandled write %08x (%d): %08x", raddr, sz, val);
}

u16 core::read_RAM(u32 addr, bool has_effect, bool triggers_irq) {
    u16 v = RAM[(addr >> 1) & 0x3FFFF];
    if (triggers_irq && has_effect) check_irq_addr(addr);
    return v;
}

u8 core::read_RAM8(u32 addr, bool has_effect, bool triggers_irq) {
    u8 v = cR8(RAM, addr);
    if ((addr & 1) == 0 && triggers_irq && has_effect) check_irq_addr(addr);
    return v;
}


void core::write_RAM(u32 addr, u16 val, bool triggers_irq) {
    RAM[(addr >> 1) & 0x3FFFF] = val;
    if (triggers_irq) check_irq_addr(addr);
}

void core::check_irq_addr(u32 addr) {
    if (io.SPUCNT.irq9_enable && addr == io.IRQ_addr && ((io.RAMCNT.mode & 6) != 0)) {
        io.SPUSTAT.irq9 = 1;
        printf("\nIRQ9 FIRE @%08X!", addr);
        update_IRQs();
    }
}
// TODO: 44.1kHz scheduling, sampling, mixing, etc.
void VOICE::keyon() {
    io.reached_loop_end = 0;
    env.phase = EP_ATTACK;
    env.adsr.output = 0;
    env.load_attack();
    adpcm.repeat_addr = adpcm.start_addr;
    adpcm_start();
}

void VOICE::keyoff() {
    env.load_release();
}

void VOICE::reset(PS1::core *ps1, u32 num_in) {
    num = num_in;
    bus = ps1;
    env.num = num_in;
}

u16 VOICE::read_reg(u32 regnum) {
    switch (regnum) {
        case 0: {
            return io.vol_l.read();
        }
        case 1: {
            return io.vol_r.read();
        }
        case 2: return io.sample_rate;
        case 3: return adpcm.start_addr >> 3;
            // 4, 5 = 8, A
        case 4: return io.env_lo;
        case 5: return io.env_hi;
        case 6: {
            return env.adsr.output;
        }

        case 7: return adpcm.repeat_addr >> 3;
        default:
            NOGOHERE;
    }
}

void VOICE::write_env_lo(u16 val) {
    bool changed = io.env_lo != val;
    io.env_lo = val;
    if (changed) {
        env.attack_exponential = (val >> 15) & 1;
        env.attack_shift = (val >> 10) & 0x1F;
        env.attack_step = (val >> 8) & 3;
        env.decay_shift = (val >> 4) & 0xF;
        env.sustain_level = ((val & 0x0F) + 1) * 0x800;
    }
}

void VOICE::write_env_hi(u16 val) {
    bool changed = io.env_hi != val;
    io.env_hi = val;
    if (changed) {
        env.sustain_exponential = (val >> 15) & 1;
        env.sustain_decreasing = (val >> 14) & 1;
        env.sustain_shift = (val >> 8) & 0x1F;
        env.sustain_step = (val >> 6) & 3;
        env.release_exponential = (val >> 5) & 1;
        env.release_shift = val & 0x1F;
    }
}

void VOICE::write_reg(u32 regnum, u16 val) {
    switch (regnum) {
        case 0: io.vol_l.write(val); return;
        case 1: io.vol_r.write(val); return;
        case 2: io.sample_rate = val & 0x7FFF; return;
        case 3: {
                adpcm.start_addr = val << 3;
            return;
        }
        case 4: write_env_lo(val); return;
        case 5: write_env_hi(val); return;
        case 6: {
            env.adsr.output = static_cast<i16>(val); return;
            // ?? psx-spx says both it works and not
            return; // WHICH IS IT!?
        }
        case 7: adpcm.repeat_addr = val << 3; return;
        default:
            NOGOHERE;
    }
}

void core::reset() {
    for (u32 i = 0; i < 24; i++) voices[i].reset(bus, i);
}

u32 core::mainbus_read(u32 addr, u8 sz, bool has_effect) {
    u32 v = snooped_mainbus_read(addr, sz, has_effect);
    //dbg_printf("\n(SPU) Read %08x(%d): %08x", addr, sz, v);
    return v;

}

u32 core::snooped_mainbus_read(u32 addr, u8 sz, bool has_effect) {
    u32 raddr = addr;
    addr &= 0xFFFFFFFE;
    u32 v;
    if (sz == 4) {
        v = mainbus_read(addr, 2, has_effect);
        v |= mainbus_read(addr + 2, 2, has_effect) << 16;
        return v;
    }
    if (sz == 1) sz = 2;
    if (addr >= 0x1F801C00 && addr <= 0x1F801D7F) {
        addr -= 0x1F801C00;
        addr %= 0x180; // 8 16-bit regs per voice
        return voices[addr >> 4].read_reg((addr & 0xF) >> 1);
    }
    if ((addr >= 0x1F801E00) && (addr <= 0x1F801E5F)) {
        // 00 = l
        // 02 = r
        addr -= 0x1F801E00;
        v = addr >> 2;
        if (addr & 2) return voices[v].io.vol_r.sweep.output;
        return voices[v].io.vol_l.sweep.output;
    }
    // D84...DFE reverb
    if (addr >= 0x1F801DA4 && addr <= 0x1F801DBA) {
        return read_control_regs(addr, sz);
    }
    if (addr >= 0x1F801DC0 && addr <= 0x1F801DFF) {
        return read_reverb_reg(addr, sz);
    }

    switch (addr) {
        case 0x1F801D90: return io.pmon_lo;
        case 0x1F801D92: return io.pmon_hi;
        case 0x1F801D80:
            return io.vol_L;
        case 0x1F801D82:
            return io.vol_R;
        case 0x1F801D9C:
            v = 0;
            for (u32 i = 0; i < 16; i++) {
                v |= voices[i].io.reached_loop_end << i;
            }
            return v;
        case 0x1F801D9E:
            v = 0;
            for (u32 i = 0; i < 8; i++) {
                v |= voices[i+16].io.reached_loop_end << i;
            }
            return v;
        case 0x1F801D84: return io.reverb.vol_out_l;
        case 0x1F801D86: return io.reverb.vol_out_r;
        case 0x1F801D88: {
            return io.keyon_lo;
        }
        case 0x1F801D8A: return io.keyon_hi;
        case 0x1F801D8C: return io.keyoff_lo;
        case 0x1F801D8E: return io.keyoff_hi;
        case 0x1F801D94: return io.non_lo;
        case 0x1F801D96: return io.non_hi;
        case 0x1F801D98: return io.reverb.on_lo;
        case 0x1F801D9A: return io.reverb.on_hi;
        case 0x1F801DA2: return io.reverb.mBASE >> 3;
        case 0x1F801DA0: return 0x9D78;
        case 0x1F801DBC: return 0x8021;
        case 0x1F801DBE: return 0x4BDF;
            // 1F801E60 32 bytes R/W has default value
    }
    printf("\n(SPU) Unhandled read %08x (%d)", raddr, sz);
    return masksz[sz];
}
}

