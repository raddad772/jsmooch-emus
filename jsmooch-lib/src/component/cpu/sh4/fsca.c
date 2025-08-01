//
// Created by Dave on 2/15/2024.
//

#include "fsca.h"
#include "fsca-table.h"

float SH4_sin_table[0x14000];

void generate_fsca_table()
{
    float *p = (float *)fsca_table_bin;
    for (int i=0;i<0x10000;i++)
    {
        if (i<0x8000) {
            SH4_sin_table[i] = *p;
            p++;
        }
        else if (i==0x8000)
            SH4_sin_table[i]=0;
        else// if (i > 0x8000)
            SH4_sin_table[i]=-SH4_sin_table[i-0x8000];
    }
    for (int i=0x10000;i<0x14000;i++)
    {
        SH4_sin_table[i]=SH4_sin_table[(u16)i];//wrap around for the last 0x4000 entries
    }
}


