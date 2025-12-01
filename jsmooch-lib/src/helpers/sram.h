//
// Created by . on 11/26/24.
//

#pragma once
#include <cstdio>

#include "int.h"

enum persistent_store_kind {
    PSK_RAM,
    PSK_MAPPED_FILE,
    PSK_SIMPLE_FILE
};

struct persistent_store {
    // Internal to persisten store
    void *data{};
    u64 actual_size{};

    persistent_store_kind kind{};

    // These values must be set on discovery of RAM/SRAM
    u32 fill_value{}; // Fill value to initialize with
    u64 requested_size{};
    u32 ready_to_use{}; // Set by host
    bool dirty{}; // Set by guest on dirtying it
    u32 persistent{};

    // Internal to UI
    char filename[500]{}; // Filename if present
    FILE *fno{};
    u64 old_requested_size{};

    persistent_store() = default;
    ~persistent_store();
};
