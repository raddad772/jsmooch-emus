//
// Created by . on 3/25/25.
//

#include <string.h>
#include "nds_3dmath.h"

void matrix_translate(i32 *matrix, i32 *data)
{
    matrix[12] += (i32)(((i64)data[0] * matrix[0] + (i64)data[1] * matrix[4] + (i64)data[2] * matrix[8]) >> 12);
    matrix[13] += (i32)(((i64)data[0] * matrix[1] + (i64)data[1] * matrix[5] + (i64)data[2] * matrix[9]) >> 12);
    matrix[14] += (i32)(((i64)data[0] * matrix[2] + (i64)data[1] * matrix[6] + (i64)data[2] * matrix[10]) >> 12);
    matrix[15] += (i32)(((i64)data[0] * matrix[3] + (i64)data[1] * matrix[7] + (i64)data[2] * matrix[11]) >> 12);
}

void matrix_scale(i32 *matrix, i32 *data)
{
    matrix[0] = (i32)(((i64)data[0] * matrix[0]) >> 12);
    matrix[1] = (i32)(((i64)data[0] * matrix[1]) >> 12);
    matrix[2] = (i32)(((i64)data[0] * matrix[2]) >> 12);
    matrix[3] = (i32)(((i64)data[0] * matrix[3]) >> 12);

    matrix[4] = (i32)(((i64)data[1] * matrix[4]) >> 12);
    matrix[5] = (i32)(((i64)data[1] * matrix[5]) >> 12);
    matrix[6] = (i32)(((i64)data[1] * matrix[6]) >> 12);
    matrix[7] = (i32)(((i64)data[1] * matrix[7]) >> 12);

    matrix[8] = (i32)(((i64)data[2] * matrix[8]) >> 12);
    matrix[9] = (i32)(((i64)data[2] * matrix[9]) >> 12);
    matrix[10] = (i32)(((i64)data[2] * matrix[10]) >> 12);
    matrix[11] = (i32)(((i64)data[2] * matrix[11]) >> 12);
}

void matrix_load_4x4(i32 *dest, i32 *data)
{
    memcpy(dest, data, sizeof(i32) * 16);
}

void matrix_load_4x3(i32 *dest, i32 *data)
{
    dest[0] = data[0];
    dest[1] = data[1];
    dest[2] = data[2];
    dest[3] = 0;
    dest[4] = data[3];
    dest[5] = data[4];
    dest[6] = data[5];
    dest[7] = 0;
    dest[8] = data[6];
    dest[9] = data[7];
    dest[10] = data[8];
    dest[11] = 0;
    dest[12] = data[9];
    dest[13] = data[10];
    dest[14] = data[11];
    dest[15] = 1 << 12;
}
