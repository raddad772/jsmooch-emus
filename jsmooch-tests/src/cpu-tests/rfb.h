//
// Created by RadDad772 on 2/28/24.
//

#ifndef JSMOOCH_EMUS_RFB_H
#define JSMOOCH_EMUS_RFB_H

#include <stddef.h>
#include "helpers/int.h"

struct read_file_buf {
    size_t sz;
    void *buf;
    u32 success;
};

void rfb_cleanup(struct read_file_buf *rfb);
int open_and_read(char *fname, struct read_file_buf *rfb);

#endif //JSMOOCH_EMUS_RFB_H
