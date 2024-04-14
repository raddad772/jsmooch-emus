#!/usr/bin/python3

CONV1 = """
@as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 0, .offset = 0, .gouraud = 0, .uv_16bit = 0, .col_type = .PackedColor, .shadow = 0 })) => return .PolygonType0,
@as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 0, .offset = 0, .gouraud = 0, .uv_16bit = 1, .col_type = .PackedColor, .shadow = 0 })) => return .PolygonType0, // FIXME: uv_16bit is supposed to be invalid in this case (Non-textured 16-bit uv doesn't make sense), but Ecco the Dolphin use such polygons? Or am I just receiving bad data?
                                                                                                                                                                                                                                                             @as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 0, .offset = 1, .gouraud = 0, .uv_16bit = 0, .col_type = .PackedColor, .shadow = 0 })) => return .PolygonType0, // FIXME: Same thing, offset should be fixed to 0 for non-textured polygons. (Hydro Thunder does this)
@as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 0, .offset = 1, .gouraud = 0, .uv_16bit = 1, .col_type = .PackedColor, .shadow = 0 })) => return .PolygonType0, // FIXME: Same thing, offset should be fixed to 0 for non-textured polygons.
    @as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 0, .offset = 0, .gouraud = 0, .uv_16bit = 0, .col_type = .FloatingColor, .shadow = 0 })) => return .PolygonType0,
@as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 0, .offset = 0, .gouraud = 0, .uv_16bit = 0, .col_type = .IntensityMode1, .shadow = 0 })) => return .PolygonType1,
@as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 0, .offset = 0, .gouraud = 0, .uv_16bit = 0, .col_type = .IntensityMode2, .shadow = 0 })) => return .PolygonType0,
@as(u16, @bitCast(ObjControl{ .volume = 1, .texture = 0, .offset = 0, .gouraud = 0, .uv_16bit = 0, .col_type = .PackedColor, .shadow = 0 })) => return .PolygonType3,
@as(u16, @bitCast(ObjControl{ .volume = 1, .texture = 0, .offset = 0, .gouraud = 0, .uv_16bit = 0, .col_type = .IntensityMode1, .shadow = 0 })) => return .PolygonType4,
@as(u16, @bitCast(ObjControl{ .volume = 1, .texture = 0, .offset = 0, .gouraud = 0, .uv_16bit = 0, .col_type = .IntensityMode2, .shadow = 0 })) => return .PolygonType3,
@as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 1, .offset = 0, .gouraud = 0, .uv_16bit = 0, .col_type = .PackedColor, .shadow = 0 })) => return .PolygonType0,
@as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 1, .offset = 1, .gouraud = 0, .uv_16bit = 0, .col_type = .PackedColor, .shadow = 0 })) => return .PolygonType0,
@as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 1, .offset = 0, .gouraud = 0, .uv_16bit = 1, .col_type = .PackedColor, .shadow = 0 })) => return .PolygonType0,
@as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 1, .offset = 1, .gouraud = 0, .uv_16bit = 1, .col_type = .PackedColor, .shadow = 0 })) => return .PolygonType0,
@as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 1, .offset = 0, .gouraud = 0, .uv_16bit = 0, .col_type = .FloatingColor, .shadow = 0 })) => return .PolygonType0,
@as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 1, .offset = 1, .gouraud = 0, .uv_16bit = 0, .col_type = .FloatingColor, .shadow = 0 })) => return .PolygonType0,
@as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 1, .offset = 0, .gouraud = 0, .uv_16bit = 1, .col_type = .FloatingColor, .shadow = 0 })) => return .PolygonType0,
@as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 1, .offset = 1, .gouraud = 0, .uv_16bit = 1, .col_type = .FloatingColor, .shadow = 0 })) => return .PolygonType0,
@as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 1, .offset = 0, .gouraud = 0, .uv_16bit = 0, .col_type = .IntensityMode1, .shadow = 0 })) => return .PolygonType1,
@as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 1, .offset = 1, .gouraud = 0, .uv_16bit = 0, .col_type = .IntensityMode1, .shadow = 0 })) => return .PolygonType2,
@as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 1, .offset = 0, .gouraud = 0, .uv_16bit = 1, .col_type = .IntensityMode1, .shadow = 0 })) => return .PolygonType1,
@as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 1, .offset = 1, .gouraud = 0, .uv_16bit = 1, .col_type = .IntensityMode1, .shadow = 0 })) => return .PolygonType2,
@as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 1, .offset = 0, .gouraud = 0, .uv_16bit = 0, .col_type = .IntensityMode2, .shadow = 0 })) => return .PolygonType0,
@as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 1, .offset = 1, .gouraud = 0, .uv_16bit = 0, .col_type = .IntensityMode2, .shadow = 0 })) => return .PolygonType0,
@as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 1, .offset = 0, .gouraud = 0, .uv_16bit = 1, .col_type = .IntensityMode2, .shadow = 0 })) => return .PolygonType0,
@as(u16, @bitCast(ObjControl{ .volume = 0, .texture = 1, .offset = 1, .gouraud = 0, .uv_16bit = 1, .col_type = .IntensityMode2, .shadow = 0 })) => return .PolygonType0,
@as(u16, @bitCast(ObjControl{ .volume = 1, .texture = 1, .offset = 0, .gouraud = 0, .uv_16bit = 0, .col_type = .PackedColor, .shadow = 0 })) => return .PolygonType3,
@as(u16, @bitCast(ObjControl{ .volume = 1, .texture = 1, .offset = 1, .gouraud = 0, .uv_16bit = 0, .col_type = .PackedColor, .shadow = 0 })) => return .PolygonType3,
@as(u16, @bitCast(ObjControl{ .volume = 1, .texture = 1, .offset = 0, .gouraud = 0, .uv_16bit = 1, .col_type = .PackedColor, .shadow = 0 })) => return .PolygonType3,
@as(u16, @bitCast(ObjControl{ .volume = 1, .texture = 1, .offset = 1, .gouraud = 0, .uv_16bit = 1, .col_type = .PackedColor, .shadow = 0 })) => return .PolygonType3,
@as(u16, @bitCast(ObjControl{ .volume = 1, .texture = 1, .offset = 0, .gouraud = 0, .uv_16bit = 0, .col_type = .IntensityMode1, .shadow = 0 })) => return .PolygonType4,
@as(u16, @bitCast(ObjControl{ .volume = 1, .texture = 1, .offset = 1, .gouraud = 0, .uv_16bit = 0, .col_type = .IntensityMode1, .shadow = 0 })) => return .PolygonType4,
@as(u16, @bitCast(ObjControl{ .volume = 1, .texture = 1, .offset = 0, .gouraud = 0, .uv_16bit = 1, .col_type = .IntensityMode1, .shadow = 0 })) => return .PolygonType4,
@as(u16, @bitCast(ObjControl{ .volume = 1, .texture = 1, .offset = 1, .gouraud = 0, .uv_16bit = 1, .col_type = .IntensityMode1, .shadow = 0 })) => return .PolygonType4,
@as(u16, @bitCast(ObjControl{ .volume = 1, .texture = 1, .offset = 0, .gouraud = 0, .uv_16bit = 0, .col_type = .IntensityMode2, .shadow = 0 })) => return .PolygonType3,
@as(u16, @bitCast(ObjControl{ .volume = 1, .texture = 1, .offset = 1, .gouraud = 0, .uv_16bit = 0, .col_type = .IntensityMode2, .shadow = 0 })) => return .PolygonType3,
@as(u16, @bitCast(ObjControl{ .volume = 1, .texture = 1, .offset = 0, .gouraud = 0, .uv_16bit = 1, .col_type = .IntensityMode2, .shadow = 0 })) => return .PolygonType3,
@as(u16, @bitCast(ObjControl{ .volume = 1, .texture = 1, .offset = 1, .gouraud = 0, .uv_16bit = 1, .col_type = .IntensityMode2, .shadow = 0 })) => return .PolygonType3,
"""


# paramaeter_control_to_vertex...
CONV2 = """
@as(u16, @bitCast(ObjControl{ .uv_16bit = 0, .gouraud = 0, .offset = 0, .texture = 0, .col_type = .PackedColor, .volume = 0, .shadow = 0 })) => return .Type0,
@as(u16, @bitCast(ObjControl{ .uv_16bit = 1, .gouraud = 0, .offset = 0, .texture = 0, .col_type = .PackedColor, .volume = 0, .shadow = 0 })) => return .Type0, // FIXME: uv_16bit is supposed to be invalid here (Non-textured 16-bit uv doesn't make sense). Ecco the Dolphin does this.
@as(u16, @bitCast(ObjControl{ .uv_16bit = 0, .gouraud = 0, .offset = 0, .texture = 0, .col_type = .FloatingColor, .volume = 0, .shadow = 0 })) => return .Type1,
@as(u16, @bitCast(ObjControl{ .uv_16bit = 0, .gouraud = 0, .offset = 0, .texture = 0, .col_type = .IntensityMode1, .volume = 0, .shadow = 0 })) => return .Type2,
@as(u16, @bitCast(ObjControl{ .uv_16bit = 0, .gouraud = 0, .offset = 0, .texture = 0, .col_type = .IntensityMode2, .volume = 0, .shadow = 0 })) => return .Type2,
@as(u16, @bitCast(ObjControl{ .uv_16bit = 0, .gouraud = 0, .offset = 0, .texture = 1, .col_type = .PackedColor, .volume = 0, .shadow = 0 })) => return .Type3,
@as(u16, @bitCast(ObjControl{ .uv_16bit = 1, .gouraud = 0, .offset = 0, .texture = 1, .col_type = .PackedColor, .volume = 0, .shadow = 0 })) => return .Type4,
@as(u16, @bitCast(ObjControl{ .uv_16bit = 0, .gouraud = 0, .offset = 0, .texture = 1, .col_type = .FloatingColor, .volume = 0, .shadow = 0 })) => return .Type5,
@as(u16, @bitCast(ObjControl{ .uv_16bit = 1, .gouraud = 0, .offset = 0, .texture = 1, .col_type = .FloatingColor, .volume = 0, .shadow = 0 })) => return .Type6,
@as(u16, @bitCast(ObjControl{ .uv_16bit = 0, .gouraud = 0, .offset = 0, .texture = 1, .col_type = .IntensityMode1, .volume = 0, .shadow = 0 })) => return .Type7,
@as(u16, @bitCast(ObjControl{ .uv_16bit = 0, .gouraud = 0, .offset = 0, .texture = 1, .col_type = .IntensityMode2, .volume = 0, .shadow = 0 })) => return .Type7,
@as(u16, @bitCast(ObjControl{ .uv_16bit = 1, .gouraud = 0, .offset = 0, .texture = 1, .col_type = .IntensityMode1, .volume = 0, .shadow = 0 })) => return .Type8,
@as(u16, @bitCast(ObjControl{ .uv_16bit = 1, .gouraud = 0, .offset = 0, .texture = 1, .col_type = .IntensityMode2, .volume = 0, .shadow = 0 })) => return .Type8,
@as(u16, @bitCast(ObjControl{ .uv_16bit = 0, .gouraud = 0, .offset = 0, .texture = 0, .col_type = .PackedColor, .volume = 1, .shadow = 0 })) => return .Type9,
@as(u16, @bitCast(ObjControl{ .uv_16bit = 0, .gouraud = 0, .offset = 0, .texture = 0, .col_type = .IntensityMode1, .volume = 1, .shadow = 0 })) => return .Type10,
@as(u16, @bitCast(ObjControl{ .uv_16bit = 0, .gouraud = 0, .offset = 0, .texture = 0, .col_type = .IntensityMode2, .volume = 1, .shadow = 0 })) => return .Type10,
@as(u16, @bitCast(ObjControl{ .uv_16bit = 0, .gouraud = 0, .offset = 0, .texture = 1, .col_type = .PackedColor, .volume = 1, .shadow = 0 })) => return .Type11,
@as(u16, @bitCast(ObjControl{ .uv_16bit = 1, .gouraud = 0, .offset = 0, .texture = 1, .col_type = .PackedColor, .volume = 1, .shadow = 0 })) => return .Type12,
@as(u16, @bitCast(ObjControl{ .uv_16bit = 0, .gouraud = 0, .offset = 0, .texture = 1, .col_type = .IntensityMode1, .volume = 1, .shadow = 0 })) => return .Type13,
@as(u16, @bitCast(ObjControl{ .uv_16bit = 0, .gouraud = 0, .offset = 0, .texture = 1, .col_type = .IntensityMode2, .volume = 1, .shadow = 0 })) => return .Type13,
@as(u16, @bitCast(ObjControl{ .uv_16bit = 1, .gouraud = 0, .offset = 0, .texture = 1, .col_type = .IntensityMode1, .volume = 1, .shadow = 0 })) => return .Type14,
@as(u16, @bitCast(ObjControl{ .uv_16bit = 1, .gouraud = 0, .offset = 0, .texture = 1, .col_type = .IntensityMode2, .volume = 1, .shadow = 0 })) => return .Type14,
"""

import re

mregex = re.compile("\.[\w\s]*=[.\w\d\s]*", flags=re.MULTILINE + re.IGNORECASE)

def do_it(CONV, r1, r2):
    cases = list(CONV.split("\n"))
    bmasks = {}
    for case in cases:
        case = case.strip()
        if len(case) < 5: continue
        kvs = mregex.findall(case)
        od = {}
        for kv in kvs:
            k,v = kv.split("=")
            k = k.strip()
            v = v.strip()
            if k[:1] == ".":
                k = k[1:]
            if v[:1] == ".":
                v = v[1:]
            if v == "PackedColor":
                v = "0"
            if v == "FloatingColor":
                v = "1"
            if v == "IntensityMode1":
                v = "2"
            if v == "IntensityMode2":
                v = "3"
            od[k] = int(v)
        # DCP_type2, DCP_sprite
        ret = case.split("return ")[1][1:-1].replace(r1, r2)
        if ',' in ret:
            #ret = ret.replace(',', ';')
            ret = ret.split(",")[0] + ';'
        else:
            ret = ret + ';'
        # {.'volume': 1, 'texture': 1, 'offset': 0, 'gouraud': 0, 'uv_16bit': 1, .'col_type': 3, .'shadow': 0}
        bitmask = (od['shadow'] << 7) | (od['volume'] << 6) | (od['col_type'] << 4) | (od['texture'] << 3) | (od['offset'] << 2) | (od['gouraud'] << 1) | od['uv_16bit']
        bmasks[bitmask] = ret
    return bmasks


def main():
    bmasks = do_it(CONV1, 'PolygonType','DCP_type')
    print('to poly type')
    for k,v  in bmasks.items():
        print('        case ' + hex(k) + ': return ' + v)

    print('\nto vertex type 0x79')
    bmasks = do_it(CONV2, 'Type', 'DCVP')
    for k,v in bmasks.items():
        print('        case ' + hex(k) + ': return ' + v)


if __name__ == '__main__':
    main()

    '''for case in cases:
        case = case.strip()
        if len(case) < 5: continue
        splits = case.split(".")
        bitmask = 0
        retval = ""
        cur_key = None
        odict = {}
        for substr in splits:
            if substr[:3] == "@as":
                continue
            vals = substr.split("=")
            if len(vals) == 2:
                l, r = vals
                r = r.strip()[:-1]
            if cur_key is not None:
                odict[cur_key] = retval
            if substr[:5] == "col_t":
                cur_key ='''



