//
// Created by Dave on 1/24/2024.
//

#include <string.h>
#include <stdio.h>

#include "sys_present.h"


u32 calc_stride(u32 out_width, u32 in_width)
{
    return (out_width - in_width);
}

static u32 *calc_start_pos(void *buf, u32 x_off, u32 y_off, u32 width)
{
    u32 *a = (u32 *)buf;
    return a + (y_off * width) + x_off;
}

static u32 to_RGBA(u32 val)
{
    // At some point in the past, for SDL2 on Mac, we had BGRA palettes.
    // This will convert to RGBA for WebGPU.
    u8 *v = (u8 *)&val;
    return (v[3] << 24) | (v[0] << 16) | (v[1] << 8) | (v[2]);
}

static const u32 TIA_palette[128] = {
        0xff000000, 0xff444400, 0xff702800, 0xff841800,
        0xff880000, 0xff78005c, 0xff480078, 0xff140084,
        0xff000088, 0xff00187c, 0xff002c5c, 0xff00402c,
        0xff003c00, 0xff143800, 0xff2c3000, 0xff442800,
        0xff404040, 0xff646410, 0xff844414, 0xff983418,
        0xff9c2020, 0xff8c2074, 0xff602090, 0xff302098,
        0xff1c209c, 0xff1c3890, 0xff1c4c78, 0xff1c5c48,
        0xff205c20, 0xff345c1c, 0xff4c501c, 0xff644818,
        0xff6c6c6c, 0xff848424, 0xff985c28, 0xffac5030,
        0xffb03c3c, 0xffa03c88, 0xff783ca4, 0xff4c3cac,
        0xff3840b0, 0xff3854a8, 0xff386890, 0xff387c64,
        0xff407c40, 0xff507c38, 0xff687034, 0xff846830,
        0xff909090, 0xffa0a034, 0xffac783c, 0xffc06848,
        0xffc05858, 0xffb0589c, 0xff8c58b8, 0xff6858c0,
        0xff505cc0, 0xff5070bc, 0xff5084ac, 0xff509c80,
        0xff5c9c5c, 0xff6c9850, 0xff848c4c, 0xffa08444,
        0xffb0b0b0, 0xffb8b840, 0xffbc8c4c, 0xffd0805c,
        0xffd07070, 0xffc070b0, 0xffa070cc, 0xff7c70d0,
        0xff6874d0, 0xff6888cc, 0xff689cc0, 0xff68b494,
        0xff74b474, 0xff84b468, 0xff9ca864, 0xffb89c58,
        0xffc8c8c8, 0xffd0d050, 0xffcca05c, 0xffe09470,
        0xffe08888, 0xffd084c0, 0xffb484dc, 0xff9488e0,
        0xff7c8ce0, 0xff7c9cdc, 0xff7cb4d4, 0xff7cd0ac,
        0xff8cd08c, 0xff9ccc7c, 0xffb4c078, 0xffd0b46c,
        0xffdcdcdc, 0xffe8e85c, 0xffdcb468, 0xffeca880,
        0xffeca0a0, 0xffdc9cd0, 0xffc49cec, 0xffa8a0ec,
        0xff90a4ec, 0xff90b4ec, 0xff90cce8, 0xff90e4c0,
        0xffa4e4a4, 0xffb4e490, 0xffccd488, 0xffe8cc7c,
        0xffececec, 0xfffcfc68, 0xfffcbc94, 0xfffcb4b4,
        0xffecb0e0, 0xffd4b0fc, 0xffbcb4fc, 0xffa4b8fc,
        0xffa4c8fc, 0xffa4e0fc, 0xffa4fcd4, 0xffb8fcb8,
        0xffc8fca4, 0xffe0ec9c, 0xfffce08c, 0xffffffff
};

static const u32 ZXS_palette[2][8] = {
        {0xFF000000, 0xFF0000D8, 0xFFD80000,
         0xFFD800D8, 0xFF00D800, 0xFF00D8D8,
         0xFFD8D800, 0xFFD8D8D8},
         {0xFF000000, 0xFF0000FF, 0xFFFF0000,
          0xFFFF00FF, 0xFF00FF00, 0xFF00FFFF,
          0xFFFFFF00, 0xFFFFFFFF}
};

void mac512k_present(struct physical_io_device *device, void *out_buf, u32 out_width, u32 out_height)
{
    u8* output = (u8 *)device->display.output[device->display.last_written];
    u32* imgdata = (u32 *)out_buf;
    for (u32 ry = 0; ry < 342; ry++) {
        u32 y = ry;
        for (u32 rx = 0; rx < 512; rx++) {
            u32 x = rx;
            u32 di = ((y * out_width) + x);
            u32 ulai = (y * 512) + x;

            u32 color = output[ulai];

            imgdata[di] = color ? 0xFF000000 : 0xFFFFFFFF;
        }
    }
}

void apple2_present(struct physical_io_device *device, void *out_buf, u32 x_offset, u32 y_offset, u32 out_width, u32 out_height)
{
    u16* ibuf = (u16 *)device->display.output[device->display.last_written];
    u32* img32 = calc_start_pos(out_buf, x_offset, y_offset, out_width);
    u32 stride = calc_stride(out_width, 280);
    for (u32 y = 0; y < 192; y++) {
        for (u32 x = 0; x < 280; x++) {
            u8* iptr = (void *)ibuf + ((y * 280) + x);
            u32 c = 0;
            if (*iptr) c = 0xFFFFFF;
            *img32 = 0xFF000000 | c;
            img32++;
        }
        img32 += stride;
    }
}

void zx_spectrum_present(struct physical_io_device *device, void *out_buf, u32 out_width, u32 out_height)
{
    u8* output = (u8 *)device->display.output[device->display.last_written];
    u32* imgdata = (u32 *)out_buf;
    for (u32 ry = 0; ry < 304; ry++) {
        u32 y = ry;
        for (u32 rx = 0; rx < 352; rx++) {
            u32 x = rx;
            u32 di = ((y * out_width) + x);
            u32 ulai = (y * 352) + x;

            u32 color = output[ulai];
            u32 pal = (color & 0x08) >> 3;
            color &= 7;

            imgdata[di] = to_RGBA(ZXS_palette[pal][color]);
        }
    }

}

void atari2600_present(struct physical_io_device *device, void *out_buf, u32 out_width, u32 out_height)
{
    u8* ibuf = (u8 *)device->display.output[device->display.last_written];
    for (u32 y = 0; y < 192; y++) {
        u8* iptr = (void *)ibuf + ((y * 160));
        for (u32 x = 0; x < 160; x++) {
            u32* optr = (u32 *)((u8 *)out_buf + (((y * out_width) + x) * 4));

            u32 col;
            /*if (*iptr == 19)
                col = 0xFFFFFFFF;
            else*/
                col = TIA_palette[*iptr];

            //col = ((col & 0xFF) << 24) | ((col & 0xFFFFFF00) >> 8);
            //col = (col & 0xFF00FF00) | ((col & 0xFF0000) >> 16) | ((col & 0xFF) << 16);
            *optr = col;
            iptr++;
        }
    }
}

// Translate from DMG output to 32-bit RGBA
void DMG_present(struct physical_io_device *device, void *out_buf, u32 x_offset, u32 y_offset, u32 out_width, u32 out_height, bool is_event_view_present)
{
    u16* ibuf = (u16 *)device->display.output[device->display.last_written];
    u16* mdbuf = (u16 *)device->display.output_debug_metadata[device->display.last_written];

    if (is_event_view_present) memset(out_buf, 0, out_width*out_height*4);
    //u16* ptr = ibuf;

    u32* img32 = calc_start_pos(out_buf, x_offset, y_offset, out_width);
    u32 stride = calc_stride(out_width, 160);

    for (u32 y = 0; y < 144; y++) {
        u8* out_pos_line = (out_buf + (y * out_width * 4));
        for (u32 x = 0; x < 160; x++) {
            u16* iptr = (void *)ibuf + (((y * 160) + x) * 2);
            u16* mptr = (void *)mdbuf + (((y * 160) + x) * 2);
            u8* optr = (u8 *)img32;
            img32++;

            if (is_event_view_present) {
                optr = out_pos_line + (*mptr * 4);
            }

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
        img32 += stride;
    }
}

/*function GBC_present(data, imgdata, GB_output_buffer) {
    u32 gbo = new Uint16Array(GB_output_buffer);
    for (u32 y = 0; y < 144; y++) {
        for (u32 counter = 0; counter < 160; counter++) {
            u32 ppui = (y * 160) + counter;
            u32 di = ppui * 4;
            u32 r, g, b;
            u32 o = gbo[ppui];
            r = (o & 0x1F);
            g = (o >> 5) & 0x1F;
            b = (o >> 10) & 0x1F;
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
void GBC_present(struct physical_io_device *device, void *out_buf, u32 x_offset, u32 y_offset, u32 out_width, u32 out_height, bool is_event_view_present)
{
    u16* ibuf = (u16 *)device->display.output[device->display.last_written];
    u16* mdbuf = (u16 *)device->display.output_debug_metadata[device->display.last_written];
    if (is_event_view_present) memset(out_buf, 0, out_width*out_height*4);
    //u16* ptr = ibuf;

    u32* img32 = calc_start_pos(out_buf, x_offset, y_offset, out_width);
    u32 stride = calc_stride(out_width, 160);

    for (u32 y = 0; y < 144; y++) {
        u8* out_pos_line = (out_buf + (y * out_width * 4));
        for (u32 x = 0; x < 160; x++) {
            u16* iptr = ((u16 *)ibuf) + (((y * 160) + x));
            u16* mptr = (void *)mdbuf + (((y * 160) + x) * 2);
            u8* optr = (u8 *)img32;
            img32++;

            if (is_event_view_present) {
                optr = out_pos_line + (*mptr * 4);
            }

            u32 r, g, b;
            u16 o = *iptr;
            b = (o & 0x1F);
            g = (o >> 5) & 0x1F;
            r = (o >> 10) & 0x1F;
            r = (r << 3) | (r >> 2);
            g = (g << 3) | (g >> 2);
            b = (b << 3) | (b >> 2);

            *(optr) = (u8)b;
            *(optr+1) = (u8)g;
            *(optr+2) = (u8)r;
            *(optr+3) = 255;
        }
        img32 += stride;
    }
}

static const u32 NES_palette[512] = {
        0xFF525252, 0xFF011A51, 0xFF0F0F65, 0xFF230663, 0xFF36034B, 0xFF400426, 0xFF3F0904, 0xFF321300, 0xFF1F2000, 0xFF0B2A00, 0xFF002F00, 0xFF002E0A, 0xFF00262D, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFFA0A0A0, 0xFF1E4A9D, 0xFF3837BC, 0xFF5828B8, 0xFF752194, 0xFF84235C, 0xFF822E24, 0xFF6F3F00, 0xFF515200, 0xFF316300, 0xFF1A6B05, 0xFF0E692E, 0xFF105C68, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFFFEFFFF, 0xFF699EFC, 0xFF8987FF, 0xFFAE76FF, 0xFFCE6DF1, 0xFFE070B2, 0xFFDE7C70, 0xFFC8913E, 0xFFA6A725, 0xFF81BA28, 0xFF63C446, 0xFF54C17D, 0xFF56B3C0, 0xFF3C3C3C, 0xFF000000, 0xFF000000,
        0xFFFEFFFF, 0xFFBED6FD, 0xFFCCCCFF, 0xFFDDC4FF, 0xFFEAC0F9, 0xFFF2C1DF, 0xFFF1C7C2, 0xFFE8D0AA, 0xFFD9DA9D, 0xFFC9E29E, 0xFFBCE6AE, 0xFFB4E5C7, 0xFFB5DFE4, 0xFFA9A9A9, 0xFF000000, 0xFF000000,
        0xFF4B3432, 0xFF00072E, 0xFF0B0141, 0xFF1D0042, 0xFF300031, 0xFF3B0016, 0xFF3D0300, 0xFF2F0900, 0xFF1C1000, 0xFF091600, 0xFF001800, 0xFF001500, 0xFF000D11, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFF94716C, 0xFF172767, 0xFF301C84, 0xFF4F1385, 0xFF6B116B, 0xFF7C1541, 0xFF7E2116, 0xFF6A2C00, 0xFF4C3800, 0xFF2D4200, 0xFF164500, 0xFF0A4010, 0xFF083239, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFFEEBCB5, 0xFF5D67B0, 0xFF7C59CF, 0xFFA04FD1, 0xFFC04BB4, 0xFFD35184, 0xFFD66051, 0xFFBF6D25, 0xFF9D7B0F, 0xFF79870E, 0xFF5C8A22, 0xFF4B8449, 0xFF49757B, 0xFF362321, 0xFF000000, 0xFF000000,
        0xFFEEBCB5, 0xFFAF98B3, 0xFFBD91C0, 0xFFCD8DC0, 0xFFDB8BB5, 0xFFE38ea1, 0xFFE4948A, 0xFFDA9A75, 0xFFCCA169, 0xFFBCA569, 0xFFAFA774, 0xFFA7A486, 0xFFA69E9D, 0xFF9D7873, 0xFF000000, 0xFF000000,
        0xFF2E4527, 0xFF00132C, 0xFF00073A, 0xFF0A0035, 0xFF150020, 0xFF1F0008, 0xFF210300, 0xFF190D00, 0xFF0C1900, 0xFF012500, 0xFF002C00, 0xFF002800, 0xFF001F13, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFF678B5C, 0xFF083D64, 0xFF182A79, 0xFF2D1A72, 0xFF401251, 0xFF501629, 0xFF522204, 0xFF463400, 0xFF314800, 0xFF1C5900, 0xFF0C6400, 0xFF025E13, 0xFF01503D, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFFAEE19D, 0xFF3C89A6, 0xFF5272BD, 0xFF6C5FB6, 0xFF815492, 0xFF945A64, 0xFF966834, 0xFF897E12, 0xFF719504, 0xFF56A908, 0xFF42B621, 0xFF33AF48, 0xFF319F7A, 0xFF1E3118, 0xFF000000, 0xFF000000,
        0xFFAEE19D, 0xFF7DBBA1, 0xFF86B1AA, 0xFF92A9A7, 0xFF9BA499, 0xFFA3A785, 0xFFA4AD70, 0xFF9EB75F, 0xFF94C156, 0xFF89CA59, 0xFF7FCF66, 0xFF78CC79, 0xFF77C58F, 0xFF6E9362, 0xFF000000, 0xFF000000,
        0xFF2D2E1E, 0xFF000421, 0xFF00002F, 0xFF0A002D, 0xFF15001D, 0xFF1F0006, 0xFF200000, 0xFF180500, 0xFF0B0C00, 0xFF001300, 0xFF001600, 0xFF001300, 0xFF000B0C, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFF66664D, 0xFF072352, 0xFF181767, 0xFF2C0E64, 0xFF3F094C, 0xFF4F0D25, 0xFF511802, 0xFF442400, 0xFF303100, 0xFF1B3C00, 0xFF0B4200, 0xFF013C09, 0xFF002F2F, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFFACAC89, 0xFF3B5F8E, 0xFF5150A5, 0xFF6A44A2, 0xFF803E87, 0xFF93445A, 0xFF95522C, 0xFF86600D, 0xFF6E7001, 0xFF557D02, 0xFF408312, 0xFF317D36, 0xFF2F6E66, 0xFF1D1D11, 0xFF000000, 0xFF000000,
        0xFFACAC89, 0xFF7B8B8B, 0xFF858594, 0xFF907F93, 0xFF9A7D88, 0xFFA17F75, 0xFFA28560, 0xFF9C8C51, 0xFF929348, 0xFF869849, 0xFF7D9B53, 0xFF769865, 0xFF75927A, 0xFF6C6C53, 0xFF000000, 0xFF000000,
        0xFF373759, 0xFF000F53, 0xFF090868, 0xFF170063, 0xFF24004B, 0xFF2A0028, 0xFF260007, 0xFF1A0300, 0xFF090A00, 0xFF001300, 0xFF001A00, 0xFF001B0E, 0xFF001731, 0xFF000000, 0xFF000001, 0xFF000001,
        0xFF7575AB, 0xFF1337A1, 0xFF2C2AC0, 0xFF431BB8, 0xFF571394, 0xFF61125F, 0xFF5B172A, 0xFF482106, 0xFF2C2D00, 0xFF173D00, 0xFF08480D, 0xFF034937, 0xFF06436E, 0xFF000001, 0xFF000001, 0xFF000001,
        0xFFC2C1FF, 0xFF507BFF, 0xFF6E6CFF, 0xFF895AFF, 0xFFA04FF5, 0xFFAB4DBA, 0xFFA5547B, 0xFF8F614A, 0xFF6F6F30, 0xFF558336, 0xFF408E55, 0xFF38908A, 0xFF3D89CA, 0xFF252542, 0xFF000001, 0xFF000001,
        0xFFC2C1FF, 0xFF91A4FF, 0xFF9F9DFF, 0xFFAA95FF, 0xFFB490FF, 0xFFB98FEA, 0xFFB692CF, 0xFFAD98B8, 0xFF9F9FAB, 0xFF93A7AE, 0xFF8AACBD, 0xFF86ADD6, 0xFF88AAF1, 0xFF7D7CB4, 0xFF000001, 0xFF000001,
        0xFF322735, 0xFF00042F, 0xFF050043, 0xFF120041, 0xFF1E0030, 0xFF250017, 0xFF240001, 0xFF180000, 0xFF080600, 0xFF000C00, 0xFF000F00, 0xFF000E00, 0xFF000A12, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFF6D5B73, 0xFF0D2169, 0xFF251686, 0xFF3B0D84, 0xFF4F096A, 0xFF5A0A43, 0xFF58101B, 0xFF451A00, 0xFF2A2600, 0xFF153000, 0xFF063600, 0xFF003414, 0xFF012C3B, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFFB69DBE, 0xFF455AB4, 0xFF634CD4, 0xFF7D40D1, 0xFF943BB5, 0xFFA03C88, 0xFF9F4559, 0xFF89512C, 0xFF695F15, 0xFF506C16, 0xFF3C722B, 0xFF317051, 0xFF33677F, 0xFF211824, 0xFF000000, 0xFF000000,
        0xFFB69DBE, 0xFF8581BA, 0xFF937AC7, 0xFF9E75C6, 0xFFA872BA, 0xFFAD73A8, 0xFFAC7793, 0xFFA37D7E, 0xFF958371, 0xFF8A8872, 0xFF818B7D, 0xFF7C8A8F, 0xFF7C86A3, 0xFF73627A, 0xFF000000, 0xFF000000,
        0xFF242F30, 0xFF000A2F, 0xFF00033D, 0xFF070039, 0xFF120023, 0xFF19000C, 0xFF180000, 0xFF100100, 0xFF050800, 0xFF001100, 0xFF001700, 0xFF001601, 0xFF001117, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFF57686A, 0xFF042D68, 0xFF14207E, 0xFF281277, 0xFF3B0A56, 0xFF450B30, 0xFF44120C, 0xFF371D00, 0xFF232900, 0xFF0F3900, 0xFF024401, 0xFF00421C, 0xFF003943, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFF97AFB2, 0xFF336BB0, 0xFF485CC8, 0xFF614AC0, 0xFF773F9B, 0xFF834170, 0xFF814A43, 0xFF725821, 0xFF5B6711, 0xFF427A16, 0xFF2F8631, 0xFF258457, 0xFF277A86, 0xFF161E20, 0xFF000000, 0xFF000000,
        0xFF97AFB2, 0xFF6B92B1, 0xFF758CBB, 0xFF8083B8, 0xFF897FA9, 0xFF8E7F96, 0xFF8E8382, 0xFF888A72, 0xFF7E9169, 0xFF73996C, 0xFF6A9E7A, 0xFF659D8B, 0xFF6599A0, 0xFF5D6E71, 0xFF000000, 0xFF000000,
        0xFF252525, 0xFF000323, 0xFF000031, 0xFF07002F, 0xFF120020, 0xFF190009, 0xFF180000, 0xFF100000, 0xFF050500, 0xFF000B00, 0xFF000F00, 0xFF000E00, 0xFF00090D, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFF585858, 0xFF051F56, 0xFF14136B, 0xFF280A68, 0xFF3B0650, 0xFF45072B, 0xFF440E08, 0xFF371800, 0xFF242400, 0xFF102F00, 0xFF023500, 0xFF00330E, 0xFF002B32, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFF989898, 0xFF335696, 0xFF4847AC, 0xFF613CAA, 0xFF77368F, 0xFF833864, 0xFF814038, 0xFF734E18, 0xFF5C5D09, 0xFF43690A, 0xFF30701D, 0xFF266E40, 0xFF27646D, 0xFF161616, 0xFF000000, 0xFF000000,
        0xFF989898, 0xFF6C7C97, 0xFF7675A0, 0xFF81709F, 0xFF8A6D94, 0xFF8F6E82, 0xFF8E726E, 0xFF88785E, 0xFF7E7F56, 0xFF738457, 0xFF6A8761, 0xFF658672, 0xFF668286, 0xFF5E5E5E, 0xFF000000, 0xFF000000
};

void NES_present(struct physical_io_device *device, void *out_buf, u32 x_offset, u32 y_offset, u32 out_width, u32 out_height)
{
    u16* neso = (u16 *)device->display.output[device->display.last_written];

    u32* img32 = calc_start_pos(out_buf, x_offset, y_offset, out_width);
    u32 stride = calc_stride(out_width, 256);

    for (u32 nes_y = 0; nes_y < 240; nes_y++) {
        u32 nesyw = nes_y * 256;
        for (u32 nes_x = 0; nes_x < 256; nes_x++) {
            u32 b_nes = (nesyw + nes_x);

            u32 r = to_RGBA(NES_palette[neso[b_nes]]);
            *img32 = r;
            img32++;
        }
        img32 += stride;
    }
}

/*
 * function SMS_present(data, imgdata, SMS_output_buffer, rendered_lines) {
    u32 output = new Uint8Array(SMS_output_buffer)
    for (u32 ry = 0; ry < data.bottom_rendered_line; ry++) {
        u32 y = ry;
        for (u32 rx = 0; rx < 256; rx++) {
            u32 counter = rx;
            u32 di = ((y * 256) + counter) * 4;
            u32 ulai = (y * 256) + counter;

            u32 color = output[ulai];
            u32 r, g, b;
            b = ((color >> 4) & 3) * 0x55;
            g = ((color >> 2) & 3) * 0x55;
            r = (color & 3) * 0x55;

            imgdata[di] = r;
            imgdata[di+1] = g;
            imgdata[di+2] = b;
            imgdata[di+3] = 255;
        }
    }
}

 */

void genesis_present(struct physical_io_device *device, void *out_buf, u32 out_width, u32 out_height)
{
    //u16 *smso = (u16 *)device->display.output[device->display.last_written];
    u16 *smso = (u16 *)device->display.output[0];
    u32 w = out_width;//256 - (overscan_left + overscan_right);
    u8 *img8 = (u8 *) out_buf;
    for (u32 ry = 0; ry < 224; /*data.bottom_rendered_line; */ ry++) {
        u32 y = ry;
        u32 outyw = y * w;
        for (u32 rx = 0; rx < 320; rx++) {
            u32 x = rx;
            u32 di = ((y * 320) + x);
            u32 b_out = (outyw + x) * 4;

            u32 color = smso[di];
            //printf("\nCOLOR? %d", color);
            u32 r, g, b;
            b = (color & 15) * 16;
            g = (color & 15) * 16;
            r = (color & 15) * 16;
            img8[b_out] = b;
            img8[b_out+1] = g;
            img8[b_out+2] = r;
            img8[b_out+3] = 255;
        }
    }
}


void SMS_present(struct physical_io_device *device, void *out_buf, u32 x_offset, u32 y_offset, u32 out_width, u32 out_height)
{
    u16 *smso = (u16 *)device->display.output[device->display.last_written];

    u32* img32 = calc_start_pos(out_buf, x_offset, y_offset, out_width);
    u32 stride = calc_stride(out_width, 256);

    for (u32 y = 0; y < 192; /*data.bottom_rendered_line; */ y++) {
        for (u32 x = 0; x < 256; x++) {
            u32 di = ((y * 256) + x);

            u32 color = smso[di];
            u32 r, g, b;
            b = ((color >> 4) & 3) * 0x55;
            g = ((color >> 2) & 3) * 0x55;
            r = (color & 3) * 0x55;

            u8 *img8 = (u8 *)img32;
            img8[0] = r;
            img8[1] = g;
            img8[2] = b;
            img8[3] = 255;
            img32++;
        }
        img32 += stride;
    }
}


void GG_present(struct physical_io_device *device, void *out_buf, u32 x_offset, u32 y_offset, u32 out_width, u32 out_height) {
    u16 *smso = (u16 *) device->display.output[device->display.last_written];

    u32 *img32 = calc_start_pos(out_buf, x_offset, y_offset, out_width);
    u32 stride = calc_stride(out_width, 256);

    for (u32 y = 0; y < 192; /*data.bottom_rendered_line; */ y++) {
        for (u32 x = 0; x < 256; x++) {
            u32 di = ((y * 256) + x);

            u32 color = smso[di];
            u32 r, g, b;
            b = (color & 0x0F) * 0x11;
            g = ((color >> 4) & 0x0F) * 0x11;
            r = ((color >> 8) & 0x0F) * 0x11;

            u8 *img8 = (u8 *) img32;
            img8[0] = r;
            img8[1] = g;
            img8[2] = b;
            img8[3] = 255;
            img32++;
        }
        img32 += stride;
    }
}


void DC_present(struct physical_io_device *device, void *out_buf, u32 out_width, u32 out_height)
{
    u32 *dco = (u32 *)device->display.output[device->display.last_written];
    u32 w = out_width;
    u32 *img32 = (u32 *) out_buf;
    for (u32 ry = 0; ry < 480; /*data.bottom_rendered_line; */ ry++) {
        u32 y = ry;
        u32 outyw = y * w;
        for (u32 rx = 0; rx < 640; rx++) {
            u32 x = rx;
            u32 di = ((y * 640) + x);
            u32 b_out = (outyw + x);

            u32 color = dco[di];
            img32[b_out] = color;
        }
    }
}

void jsm_present(enum jsm_systems which, struct physical_io_device *display, void *out_buf, u32 x_offset, u32 y_offset, u32 out_width, u32 out_height, u32 is_event_view_present)
{
    switch(which) {
        case SYS_GENESIS:
            genesis_present(display, out_buf, out_width, out_height);
            break;
        case SYS_DMG:
            DMG_present(display, out_buf, x_offset, y_offset, out_width, out_height, is_event_view_present);
            break;
        case SYS_GBC:
            GBC_present(display, out_buf, x_offset, y_offset, out_width, out_height, is_event_view_present);
            break;
        case SYS_NES:
            NES_present(display, out_buf, x_offset, y_offset, out_width, out_height);
            break;
        case SYS_MAC128K:
        case SYS_MAC512K:
        case SYS_MACPLUS_1MB:
            mac512k_present(display, out_buf, out_width, out_height);
            break;
        case SYS_SMS1:
        case SYS_SMS2:
            SMS_present(display, out_buf, x_offset, y_offset, out_width, out_height);
            break;
        case SYS_GG:
            GG_present(display, out_buf, x_offset, y_offset, out_width, out_height);
            break;
        case SYS_DREAMCAST:
            DC_present(display, out_buf, out_width, out_height);
            break;
        case SYS_ATARI2600:
            atari2600_present(display, out_buf, out_width, out_height);
            break;
        case SYS_APPLEIIe:
            apple2_present(display, out_buf, x_offset, y_offset, out_width, out_height);
            break;
        case SYS_ZX_SPECTRUM_48K:
        case SYS_ZX_SPECTRUM_128K:
            zx_spectrum_present(display, out_buf, out_width, out_height);
            break;
        default:
            printf("\nUNKNOWN PRESENT!");
            break;
    }
}
