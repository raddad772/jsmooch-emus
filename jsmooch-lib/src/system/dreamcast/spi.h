//
// Created by RadDad772 on 3/10/24.
//

#ifndef JSMOOCH_EMUS_SPI_H
#define JSMOOCH_EMUS_SPI_H

#include "helpers_c/int.h"

struct SPI_packet_cmd
{
    u32 index;
    union
    {
        u16 data_16[6];
        u8 data_8[12];
        //Spi command structs
        union
        {
            struct
            {
                u8 cc;
                u8 prmtype  : 1 ;
                u8 expdtype : 3 ;
                //	u8 datasel	: 4 ;
                u8 other    : 1 ; //"other" data. I guess that means SYNC/ECC/EDC ?
                u8 data     : 1 ; //user data. 2048 for mode1, 2048 for m2f1, 2324 for m2f2
                u8 subh     : 1 ; //8 bytes, mode2 subheader
                u8 head     : 1 ; //4 bytes, main CDROM header
                u8 block[10];
            };

            struct
            {
                u8 b[12];
            };
        }GDReadBlock;
    };
} ;

#endif //JSMOOCH_EMUS_SPI_H
