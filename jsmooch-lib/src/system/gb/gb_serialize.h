//
// Created by . on 12/7/24.
//

#ifndef JSMOOCH_EMUS_GB_SERIALIZE_H
#define JSMOOCH_EMUS_GB_SERIALIZE_H

enum GBSS_kinds {
    GBSS_console,
    GBSS_ppu,
    GBSS_apu,
    GBSS_sm83,
    GBSS_clock,
    GBSS_cartridge
};


struct jsm_system;
struct serialized_state;
struct deserialize_ret;
void GBJ_save_state(struct jsm_system *, serialized_state *state);
void GBJ_load_state(struct jsm_system *, serialized_state *state, deserialize_ret *ret);


#endif //JSMOOCH_EMUS_GB_SERIALIZE_H
