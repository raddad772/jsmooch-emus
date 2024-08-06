//
// Created by . on 8/5/24.
//

#include "mac_floppy.h"

void mac_floppy_init(struct mac_floppy *this)
{
    generic_floppy_init(&this->disc);
}

void mac_floppy_delete(struct mac_floppy *this)
{
    generic_floppy_delete(&this->disc);
}
