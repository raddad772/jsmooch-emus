//
// Created by Dave on 1/24/2024.
//

#include "sys_present.h"
#include "stdio.h"

// Translate from DMG output to 32-bit RGBA
void DMG_present(u32 last_used_buffer, struct JSM_IOmap *iom, void *out_buf, u32 out_width, u32 out_height)
{
    u16* ibuf = (u16 *)iom->out_buffers[last_used_buffer];
    //u16* ptr = ibuf;

    for (u32 y = 0; y < 144; y++) {
        for (u32 x = 0; x < 160; x++) {
            u16* iptr = (void *)ibuf + (((y * 160) + x) * 2);
            u8* optr = (u8 *)out_buf;
            optr += (((y * out_width) + x) * 4);

            u32 r, g, b;
            u16 o = *iptr;
            r = o * 85;
            r = 255 - r;
            g = b = r;

            *(optr) = (u8)r;
            *(optr+1) = (u8)g;
            *(optr+2) = (u8)b;
            *(optr+3) = 255;
        }
    }
}


void jsm_present(enum jsm_systems which, u32 last_used_buffer, struct JSM_IOmap *iom, void *out_buf, u32 out_width, u32 out_height)
{
    switch(which) {
        case SYS_DMG:
            DMG_present(last_used_buffer, iom, out_buf, out_width, out_height);
            break;
        default:
            printf("\nUNKNOWN PRESENT!");
            break;
    }
}
