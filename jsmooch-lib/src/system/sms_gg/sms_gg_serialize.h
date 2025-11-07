//
// Created by . on 11/10/24.
//

#ifndef JSMOOCH_EMUS_SMS_GG_SERIALIZE_H
#define JSMOOCH_EMUS_SMS_GG_SERIALIZE_H

#include "helpers_c/serialize/serialize.h"
#include "helpers_c/sys_interface.h"

void SMSGGJ_save_state(struct jsm_system *, struct serialized_state *state);
void SMSGGJ_load_state(struct jsm_system *, struct serialized_state *state, struct deserialize_ret *ret);


#endif //JSMOOCH_EMUS_SMS_GG_SERIALIZE_H
