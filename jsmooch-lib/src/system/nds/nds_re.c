//
// Created by . on 3/12/25.
//

#include "nds_re.h"
#include "nds_bus.h"

static void clear_line(struct NDS *this, struct NDS_RE_LINEBUFFER *l)
{
    for (u32 x = 0; x < 256; x++) {
        l->poly_id[x] = this->re.io.clear.poly_id;
        l->rgb_top[x] = this->re.io.clear.color;
        l->rgb_bottom[x] = this->re.io.clear.color;
        l->alpha[x] = this->re.io.clear.alpha;
        l->z[x] = this->re.io.clear.z;
    }
}

void render_line(struct NDS *this, struct NDS_GE_BUFFERS *buf, i32 line_num)
{
    struct NDS_RE_LINEBUFFER *lbuf = &this->re.out.linebuffer[line_num];
    clear_line(this, lbuf);

}

void NDS_RE_render_frame(struct NDS *this)
{
    printf("\n\nRE render frame!");
    struct NDS_GE_BUFFERS *buf = &this->ge.buffers[this->ge.ge_has_buffer ^ 1];
    for (u32 i = 0; i < 192; i++)
        render_line(this, buf, i);
}

void NDS_RE_init(struct NDS *this)
{

}
