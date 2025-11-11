//
// Created by RadDad772 on 3/18/24.
//

#include "triangle.h"

void triangle_list_init(triangle_list* this)
{
    this->num_tris = 0;
}

void triangle_multi_list_clear(triangle_multi_list* this)
{
    this->num_lists = 0;
    for (u32 i = 0; i < MAX_TRI_LIST; i++) {
        this->lists[i].num_tris = 0;
    }
}

void triangle_multi_list_init(triangle_multi_list* this)
{
    this->num_lists = 0;
    for (u32 i = 0; i < MAX_TRI_LIST; i++) {
        triangle_list_init(&this->lists[i]);
    }
}
