//
// Created by RadDad772 on 2/28/24.
//
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "rfb.h"

int open_and_read(char *fname, struct read_file_buf *rfb)
{
    assert(1==2);
    /*FILE *fil = fopen(fname, "rb");
    if (fil == NULL) {
        rfb->success = 0;
        printf("\nCould not open %s", fname);
        return -1;
    }
    fseek(fil, 0L, SEEK_END);
    rfb->sz = ftell(fil);

    fseek(fil, 0L, SEEK_SET);
    rfb->buf = malloc(rfb->sz);
    fread(rfb->buf, sizeof(char), rfb->sz, fil);
    rfb->success = 1;

    fclose(fil);
    return 0;*/
}

void rfb_cleanup(struct read_file_buf *rfb)
{
    /*if (rfb->buf != NULL) {
        free(rfb->buf);
        rfb->buf = NULL;
    }
    rfb->sz = 0;*/
}

