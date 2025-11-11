//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_ARM7TDMI_DECODE_H
#define JSMOOCH_EMUS_ARM7TDMI_DECODE_H

struct ARM7TDMI;
struct thumb_instruction;
void ARM7TDMI_fill_arm_table(struct ARM7TDMI *this);
void decode_thumb(u16 opc, thumb_instruction *ins);

#endif //JSMOOCH_EMUS_ARM7TDMI_DECODE_H
