//
// Created by . on 11/10/24.
//

#ifndef JSMOOCH_EMUS_SMS_GG_SERIALIZE_H
#define JSMOOCH_EMUS_SMS_GG_SERIALIZE_H

#include "helpers/serialize/serialize.h"
#include "helpers/sys_interface.h"

void SMSGGJ_save_state(struct jsm_system *, serialized_state *state);
void SMSGGJ_load_state(struct jsm_system *, serialized_state *state, deserialize_ret *ret);


#endif //JSMOOCH_EMUS_SMS_GG_SERIALIZE_H
