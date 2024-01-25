//
// Created by Dave on 1/25/2024.
//

#include "sm83-tests.h"
#include "stdio.h"
#include "malloc.h"
#include "helpers/int.h"
#include "component/cpu/sm83/sm83.h"
#include "../json.h"

char *test_path = "C:\\dev\\personal\\jsmoo\\misc\\tests\\GeneratedTests\\sm83\\v1";

struct read_file_buf {
    size_t sz;
    void *buf;
};

int open_and_read(char *fname, struct read_file_buf *rfb)
{
    FILE *fil = fopen(fname, "rb");
    fseek(fil, 0L, SEEK_END);
    rfb->sz = ftell(fil);

    fseek(fil, 0L, SEEK_SET);
    rfb->buf = malloc(rfb->sz);
    fread(rfb->buf, sizeof(char), rfb->sz, fil);

    fclose(fil);
}

void rfb_cleanup(struct read_file_buf *rfb)
{
    if (rfb->buf != NULL) {
        free(rfb->buf);
        rfb->buf = NULL;
    }
    rfb->sz = 0;
}

void test_sm83()
{
    printf("\n")
}
