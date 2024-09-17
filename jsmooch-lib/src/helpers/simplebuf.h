//
// Created by . on 8/29/24.
//

#ifndef JSMOOCH_EMUS_SIMPLEBUF_H
#define JSMOOCH_EMUS_SIMPLEBUF_H
#ifdef __cplusplus
extern "C" {
#endif

#include "int.h"

struct simplebuf8 {
    u8 *ptr;
    u64 sz;
    u32 mask;
};

void simplebuf8_init(struct simplebuf8*);
void simplebuf8_delete(struct simplebuf8*);
void simplebuf8_allocate(struct simplebuf8*, u64 sz);
void simplebuf8_clear(struct simplebuf8 *);


#ifdef __cplusplus
}
#endif
#endif //JSMOOCH_EMUS_SIMPLEBUF_H
