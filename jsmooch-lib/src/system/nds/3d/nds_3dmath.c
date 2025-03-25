//
// Created by . on 3/25/25.
//

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
