//
// Created by . on 11/26/24.
//

#ifndef JSMOOCH_EMUS_SRAM_H
#define JSMOOCH_EMUS_SRAM_H


#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>

#include "int.h"

enum persistent_store_kind {
    PSK_RAM,
    PSK_MAPPED_FILE,
    PSK_SIMPLE_FILE
};

struct persistent_store {
    void *data;
    u64 actual_size;

    enum persistent_store_kind kind;
    u64 requested_size;

    u32 ready_to_use; // Set by host

    u32 dirty; // Set by guest on dirtying it

    char filename[500]; // Filename if present

    FILE *fno;
};

void persistent_store_init(struct persistent_store*);
void persistent_store_delete(struct persistent_store*);

#ifdef __cplusplus
}
#endif

#endif //JSMOOCH_EMUS_SRAM_H
