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
    // Internal to persisten store
    void *data;
    u64 actual_size;

    enum persistent_store_kind kind;

    // These values must be set on discovery of RAM/SRAM
    u32 fill_value; // Fill value to initialize with
    u64 requested_size;
    u32 ready_to_use; // Set by host
    u32 dirty; // Set by guest on dirtying it
    u32 persistent;

    // Internal to UI
    char filename[500]; // Filename if present
    FILE *fno;
    u64 old_requested_size;

};

void persistent_store_init(persistent_store*);
void persistent_store_delete(persistent_store*);

#ifdef __cplusplus
}
#endif

#endif //JSMOOCH_EMUS_SRAM_H
