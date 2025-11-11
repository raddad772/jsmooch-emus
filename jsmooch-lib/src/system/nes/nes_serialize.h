//
// Created by . on 12/7/24.
//

#ifndef JSMOOCH_EMUS_NES_SERIALIZE_H
#define JSMOOCH_EMUS_NES_SERIALIZE_H

enum NESSS_kinds {
    NESSS_console,
    NESSS_debug,
    NESSS_ppu,
    NESSS_cpu,
    NESSS_apu,
    NESSS_clock,
    NESSS_cartridge,
};


struct jsm_system;
struct serialized_state;
struct deserialize_ret;
void NESJ_save_state(jsm_system *, serialized_state *state);
void NESJ_load_state(jsm_system *, serialized_state *state, deserialize_ret *ret);

#endif //JSMOOCH_EMUS_NES_SERIALIZE_H
