//
// Created by . on 3/12/25.
//

#include <stdlib.h>
#include <math.h>
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

struct NDS_RE_EDGE {
    struct NDS_RE_VERTEX *v[2];
    i32 num[2];
    i32 dir;
    u32 found;
};

#define DBGL 165

void find_closest_points_marked(struct NDS *this, struct NDS_GE_BUFFERS *b, struct NDS_RE_POLY *p, u32 comp_y, u32 marked_as, struct NDS_RE_EDGE *edges)
{
    struct NDS_RE_VERTEX *first_vertex = &b->vertex[p->first_vertex_ptr + p->highest_vertex];
    if (comp_y == DBGL)
        printf("\nHIGHEST VERTEX %d, %d", first_vertex->xx, first_vertex->yy);

    edges[0].num[1] = p->highest_vertex;
    edges[1].num[1] = p->highest_vertex;
    edges[0].v[1] = first_vertex;
    edges[1].v[1] = first_vertex;
    edges[0].dir = -1;
    edges[1].dir = 1;
    edges[0].found = edges[1].found = 0;
    u32 total_found = 0;

    for (u32 i = 0; i < p->num_vertices; i++) {
        for (i32 j = 0; j < 2; j++) {
            if (edges[j].found) continue;
            edges[j].num[0] = edges[j].num[1];
            edges[j].v[0] = edges[j].v[1];

            edges[j].num[1] = (edges[j].num[1] + edges[j].dir) % p->num_vertices;
            edges[j].v[1] = &b->vertex[edges[j].num[1] + p->first_vertex_ptr];

            // TODO: verify vs. winding order if slope is positive or negative properly?
            // original though was just to verify direction but that doesn't work...
            //if (edges[j].v[0]->yy > edges[j].v[1]->yy) continue;

            // Check if edge intersects scanline
            i32 min_y = edges[j].v[0]->yy;
            i32 max_y = edges[j].v[1]->yy;
            if (max_y < min_y) {
                i32 t = min_y;
                min_y = max_y;
                max_y = t;
            }

            if ((min_y >= comp_y) || (max_y < comp_y)) continue;

            total_found++;
            edges[j].found = 1;
        }
        if (total_found == 2) break;
    }
    for (u32 j = 0; j < 2; j++) {
        if (edges[j].v[0]->yy > edges[j].v[1]->yy) {
            struct NDS_RE_VERTEX *t = edges[j].v[0];
            edges[j].v[0] = edges[j].v[1];
            edges[j].v[1] = t;
        }
    }
}

static void lerp_edge_to_vtx(struct NDS_RE_EDGE *e, struct NDS_RE_VERTEX *v, i32 y)
{
    float y_diff = e->v[1]->yy - e->v[0]->yy;
    float x_diff = e->v[1]->xx - e->v[0]->xx;
    float w_diff = e->v[1]->ww - e->v[0]->ww;

    float x_slope = x_diff / y_diff;
    float x_loc = x_slope * (float)(y - e->v[0]->yy);
    x_loc += e->v[0]->xx;

    // y_diff = 10 lines
    // x from 1 to 5
    // (4/10 * 4) + 1 = 2.6
    // (

    v->yy = y;
    v->xx = (u32)x_loc;
}

void render_line(struct NDS *this, struct NDS_GE_BUFFERS *b, i32 line_num)
{
    struct NDS_RE_LINEBUFFER *line = &this->re.out.linebuffer[line_num];
    printf("\n\nLine num %d", line_num);
    clear_line(this, line);
    u32 test_byte = line_num >> 3;
    u32 test_bit = line_num & 7;

    struct NDS_RE_EDGE edges[2];
    for (u32 poly_num = 0; poly_num < b->polygon_index; poly_num++) {
        struct NDS_RE_POLY *p = &b->polygon[poly_num];


        line->rgb_top[10] = 0x1F;
        // Polygon does not intersect this line
        if (!(p->lines_on_bitfield[test_byte] & (1 << test_bit))) {
            continue;
        }

        // Find the correct points to lerp
        find_closest_points_marked(this, b, p, line_num, 0, edges);

        struct NDS_RE_VERTEX lerped[2];

        lerp_edge_to_vtx(&edges[0], &lerped[0], line_num);
        lerp_edge_to_vtx(&edges[1], &lerped[1], line_num);
        u32 xorby = lerped[0].xx > lerped[1].xx ? 1 : 0;
        struct NDS_RE_VERTEX *left = &lerped[0 ^ xorby];
        struct NDS_RE_VERTEX *right = &lerped[1 ^ xorby];

        printf("\nTop left   : %d %d    Top right   : %d %d", edges[0 ^ xorby].v[0]->xx, edges[0 ^ xorby].v[0]->yy, edges[1 ^ xorby].v[0]->xx, edges[1 ^ xorby].v[0]->yy);
        printf("\nBottom left: %d %d    Bottom right: %d %d ", edges[0 ^ xorby].v[1]->xx, edges[0 ^ xorby].v[1]->yy, edges[1 ^ xorby].v[1]->xx, edges[1 ^ xorby].v[1]->yy);
        printf("\nleft %d, right %d", left->xx, right->xx);

        for (u32 x = left->xx; x < right->xx; x++) {
            if (x < 256) {
                line->rgb_top[x] = 0x7FFF;
            }
        }
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
