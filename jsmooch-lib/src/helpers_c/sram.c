//
// Created by . on 11/26/24.
//

#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "sram.h"

void persistent_store_init(persistent_store*this)
{
    memset(this, 0, sizeof(persistent_store));
}

void persistent_store_delete(persistent_store*this)
{
    if (this->dirty && this->requested_size > 0){
        printf("\nWARNING: DELETING DIRTY PERSISTENT STORE!");
    }
    this->ready_to_use = 0;
}
