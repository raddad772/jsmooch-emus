//
// Created by . on 11/29/24.
//

#ifndef JSMOOCH_EMUS_GENESIS_SERIALIZE_H
#define JSMOOCH_EMUS_GENESIS_SERIALIZE_H

enum genesisSS_kinds {
    genesisSS_console,
    genesisSS_debug,
    genesisSS_vdp,
    genesisSS_sn76489,
    genesisSS_m68k,
    genesisSS_z80,
    genesisSS_clock,
    genesisSS_cartridge,
    genesisSS_ym2612
};


struct jsm_system;
struct serialized_state;
struct deserialize_ret;
void genesisJ_save_state(struct jsm_system *, struct serialized_state *state);
void genesisJ_load_state(struct jsm_system *, struct serialized_state *state, struct deserialize_ret *ret);

#endif //JSMOOCH_EMUS_GENESIS_SERIALIZE_H
