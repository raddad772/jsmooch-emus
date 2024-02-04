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

/*function GBC_present(data, imgdata, GB_output_buffer) {
    let gbo = new Uint16Array(GB_output_buffer);
    for (let y = 0; y < 144; y++) {
        for (let x = 0; x < 160; x++) {
            let ppui = (y * 160) + x;
            let di = ppui * 4;
            let r, g, b;
            let o = gbo[ppui];
            r = (o & 0x1F);
            g = (o >>> 5) & 0x1F;
            b = (o >>> 10) & 0x1F;
            r = (r << 3) | (r >> 2);
            g = (g << 3) | (g >> 2);
            b = (b << 3) | (b >> 2);
            imgdata[di] = r;
            imgdata[di+1] = g;
            imgdata[di+2] = b;
            imgdata[di+3] = 255;
        }
    }
    // draw lines around screen
    //this.draw_lines_around_screen(imgdata);
}
*/
void GBC_present(u32 last_used_buffer, struct JSM_IOmap *iom, void *out_buf, u32 out_width, u32 out_height)
{
    u16* ibuf = (u16 *)iom->out_buffers[last_used_buffer];
    //u16* ptr = ibuf;

    for (u32 y = 0; y < 144; y++) {
        for (u32 x = 0; x < 160; x++) {
            u16* iptr = ((u16 *)ibuf) + (((y * 160) + x));
            u8* optr = (u8 *)out_buf;
            optr += (((y * out_width) + x) * 4);

            u32 r, g, b;
            u16 o = *iptr;
            b = (o & 0x1F);
            g = (o >> 5) & 0x1F;
            r = (o >> 10) & 0x1F;
            r = (r << 3) | (r >> 2);
            g = (g << 3) | (g >> 2);
            b = (b << 3) | (b >> 2);

            /*r *= 255;
            g *= 255;
            b *= 255;*/

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
        case SYS_GBC:
            GBC_present(last_used_buffer, iom, out_buf, out_width, out_height);
            break;
        default:
            printf("\nUNKNOWN PRESENT!");
            break;
    }
}
