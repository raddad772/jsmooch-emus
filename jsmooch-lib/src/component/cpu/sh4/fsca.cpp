//
// Created by Dave on 2/15/2024.
//

#include "fsca.h"
#include "fsca-table.h"
namespace SH4 {
float sin_table[0x14000];

void generate_fsca_table()
{
    float *p = reinterpret_cast<float *>(fsca_table_bin);
    for (int i=0;i<0x10000;i++)
    {
        if (i<0x8000) {
            sin_table[i] = *p;
            p++;
        }
        else if (i==0x8000)
            sin_table[i]=0;
        else// if (i > 0x8000)
            sin_table[i]=-sin_table[i-0x8000];
    }
    for (int i=0x10000;i<0x14000;i++)
    {
        sin_table[i]=sin_table[(u16)i];//wrap around for the last 0x4000 entries
    }
}


}