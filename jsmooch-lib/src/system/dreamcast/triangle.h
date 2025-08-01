//
// Created by RadDad772 on 3/18/24.
//

#ifndef JSMOOCH_EMUS_TRIANGLE_H
#define JSMOOCH_EMUS_TRIANGLE_H

#define MAX_TRI 50
#define MAX_TRI_LIST 10

#include "helpers/int.h"

struct point {
    float x, y, z, u, v;
    u8 r, g, b;
};

struct triangle {
    struct point points[3];
    u32 gouraud;
    u32 textured;
    u8 r, g, b;
};

struct triangle_list {
    u32 num_tris;
    struct triangle triangles[MAX_TRI];
};

struct triangle_multi_list {
    u32 num_lists;
    struct triangle_list lists[MAX_TRI_LIST];
};


void triangle_list_init(struct triangle_list*);
void triangle_multi_list_init(struct triangle_multi_list*);
void triangle_multi_list_clear(struct triangle_multi_list*);
#endif //JSMOOCH_EMUS_TRIANGLE_H
