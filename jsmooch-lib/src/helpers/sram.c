//
// Created by . on 11/26/24.
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "sram.h"

void persistent_store_init(struct persistent_store*this)
{
    memset(this, 0, sizeof(struct persistent_store));
}

void persistent_store_delete(struct persistent_store*this)
{
    if (this->dirty && this->requested_size > 0){
        printf("\nWARNING: DELETING DIRTY PERSISTENT STORE!");
    }
    this->ready_to_use = 0;
}
