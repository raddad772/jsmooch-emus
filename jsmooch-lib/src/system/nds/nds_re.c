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

#define CCW 0
#define CW 1

void find_closest_points_marked(struct NDS *this, struct NDS_GE_BUFFERS *b, struct NDS_RE_POLY *p, u32 comp_y, u32 marked_as, struct NDS_RE_VERTEX *ptrs[])
{
    struct NDS_RE_VERTEX *first_vertex = &b->vertex[p->first_vertex_ptr];
    i32 cur_idx = p->highest_vertex;

    struct NDS_RE_VERTEX *bottom = &b->vertex[p->first_vertex_ptr + cur_idx];
    struct NDS_RE_VERTEX *top = bottom;

    // Follow edge marked_as down from top based on winding order
    i32 dir = marked_as ? -1 : 1;
    for (u32 i = 0; i < p->num_vertices; i++) {
        // Evaluate current vertex as well as its previous and next to determine if we can use it
        i32 prev_idx = (i32)cur_idx - 1;
        if (prev_idx < 0) prev_idx += p->num_vertices;
        i32 next_idx = (cur_idx + 1) % p->num_vertices;

        u32 edge_was = (p->edge_r_bitfield >> prev_idx) & 1;
        u32 edge_is = (p->edge_r_bitfield >> cur_idx) & 1;
        u32 edge_will_be = (p->edge_r_bitfield >> next_idx) & 1;
        if ((edge_was == marked_as) || (edge_is == marked_as) || (edge_will_be == marked_as)) {
            struct NDS_RE_VERTEX *cmp = &b->vertex[p->first_vertex_ptr + cur_idx];
            // Determine stuff!
            if (cmp->yy == top->yy) { // horizontal line, take left or rightmost
                if ((marked_as == 0) && (cmp->xx < top->xx)) top = cmp;
                else if ((marked_as == 1) && (cmp->xx > top->xx)) top = cmp;
            }
            if (cmp->yy > top->yy) { // it's moving down...

            }
        }
        // If we can, evaluate vs. top
        // Evaluate vs. bottom
        // Move to next vertex either +1 or -1
    }
}

void render_line(struct NDS *this, struct NDS_GE_BUFFERS *b, i32 line_num)
{
    struct NDS_RE_LINEBUFFER *line = &this->re.out.linebuffer[line_num];
    clear_line(this, line);
    u32 test_byte = line_num >> 3;
    u32 test_bit = line_num & 7;

    struct NDS_RE_VERTEX *pointers[4];
    for (u32 poly_num = 0; poly_num < b->polygon_index; poly_num++) {
        struct NDS_RE_POLY *p = &b->polygon[poly_num];

        // Polygon does not intersect this line
        if (!(p->lines_on_bitfield[test_byte] & test_bit)) continue;

        // Find the closest left and right points above and below y
        find_closest_points_marked(this, b, p, line_num, 0, pointers);
        find_closest_points_marked(this, b, p, line_num, 1, pointers+2);
    }
}

void NDS_RE_render_frame(struct NDS *this)
{
    struct NDS_GE_BUFFERS *b = &this->ge.buffers[this->ge.ge_has_buffer ^ 1];
    for (u32 i = 0; i < 192; i++)
        render_line(this, b, i);
}

void NDS_RE_init(struct NDS *this)
{

}

void NDS_RE_reset(struct NDS *this)
{

}
