//
// Created by RadDad772 on 3/2/24.
//

#ifndef JSMOOCH_EMUS_GDROM_H
#define JSMOOCH_EMUS_GDROM_H

#include "helpers_c/int.h"
#include "dreamcast.h"
#include "dc_mem.h"

enum gd_states {
    //Generic
    gds_waitcmd,
    gds_procata,
    gds_waitpacket,
    gds_procpacket,
    gds_pio_send_data,
    gds_pio_get_data,
    gds_pio_end,
    gds_procpacketdone,

    //Command spec.
    gds_readsector_pio,
    gds_readsector_dma,
    gds_process_set_mode,
};

void GDROM_write(struct DC*, u32 reg, u64 val, u32 sz, u32* success);
u64 GDROM_read(struct DC*, u32 reg, u32 sz, u32* success);
void GDROM_reset(struct DC*);
void GDROM_init(struct DC*);

#endif //JSMOOCH_EMUS_GDROM_H
