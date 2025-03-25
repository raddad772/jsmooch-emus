//
// Created by . on 3/25/25.
//

#ifndef JSMOOCH_EMUS_NDS_3DMATH_H
#define JSMOOCH_EMUS_NDS_3DMATH_H

#include "helpers/int.h"


void matrix_translate(i32 *matrix, i32 *data);
void matrix_scale(i32 *matrix, i32 *data);
void matrix_load_4x3(i32 *dest, i32 *data);
void matrix_load_4x4(i32 *dest, i32 *data);

#endif //JSMOOCH_EMUS_NDS_3DMATH_H
