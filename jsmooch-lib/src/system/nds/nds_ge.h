//
// Created by . on 3/9/25.
//

#ifndef JSMOOCH_EMUS_NDS_GE_H
#define JSMOOCH_EMUS_NDS_GE_H

#include "helpers/int.h"

enum NDS_GE_cmds {
    NDS_GE_CMD_NOP = 0x00,
    NDS_GE_CMD_MTX_MODE = 0x10,
    NDS_GE_CMD_MTX_PUSH = 0x11,
    NDS_GE_CMD_MTX_POP = 0x12,
    NDS_GE_CMD_MTX_STORE = 0x13,
    NDS_GE_CMD_MTX_RESTORE = 0x14,
    NDS_GE_CMD_MTX_IDENTITY = 0x15,
    NDS_GE_CMD_MTX_LOAD_4x4 = 0x16,
    NDS_GE_CMD_MTX_LOAD_4x3 = 0x17,
    NDS_GE_CMD_MTX_MULT_4x4 = 0x18,
    NDS_GE_CMD_MTX_MULT_4x3 = 0x19,
    NDS_GE_CMD_MTX_MULT_3x3 = 0x1A,
    NDS_GE_CMD_MTX_SCALE = 0x1B,
    NDS_GE_CMD_MTX_TRANS = 0x1C,
    NDS_GE_CMD_COLOR = 0x20,
    NDS_GE_CMD_NORMAL = 0x21,
    NDS_GE_CMD_TEXCOORD = 0x22,
    NDS_GE_CMD_VTX_16 = 0x23,
    NDS_GE_CMD_VTX_10 = 0x24,
    NDS_GE_CMD_VTX_XY = 0x25,
    NDS_GE_CMD_VTX_XZ = 0x26,
    NDS_GE_CMD_VTX_YZ = 0x27,
    NDS_GE_CMD_VTX_DIFF = 0x28,
    NDS_GE_CMD_POLYGON_ATTR = 0x29,
    NDS_GE_CMD_TEXIMAGE_PARAM = 0x2A,
    NDS_GE_CMD_PLTT_BASE = 0x2B,
    NDS_GE_CMD_DIF_AMB = 0x30,
    NDS_GE_CMD_SPE_EMI = 0x31,
    NDS_GE_CMD_LIGHT_VECTOR = 0x32,
    NDS_GE_CMD_LIGHT_COLOR = 0x33,
    NDS_GE_CMD_SHININESS = 0x34,
    NDS_GE_CMD_BEGIN_VTXS = 0x40,
    NDS_GE_CMD_END_VTXS = 0x41,
    NDS_GE_CMD_SWAP_BUFFERS = 0x50,
    NDS_GE_CMD_VIEWPORT = 0x60,
    NDS_GE_CMD_BOX_TEST = 0x70,
    NDS_GE_CMD_POS_TEST = 0x71,
    NDS_GE_CMD_VEC_TEST = 0x72,
};


struct NDS_GE_matrix {
    i32 m[16];
};

struct NDS_GE_FIFO_entry {
    u8 cmd;
    u32 data;
};

enum NDS_GE_POLY_MODE {
    NDS_GE_PM_modulation,
    NDS_GE_PM_decal,
    NDS_GE_PM_toon_hightlight,
    NDS_GE_PM_shadow
};

enum NDS_RE_DEPTH_TEST_MODE {
    NDS_RE_DTM_LESS_THAN,
    NDS_RE_DTM_EQUAL
};


enum NDS_TEX_FORMAT {
    NDS_TF_none,
    NDS_TF_a3i5_translucent,
    NDS_TF_bpp2,
    NDS_TF_bpp4,
    NDS_TF_bpp8,
    NDS_TF_4x4_compressed,
    NDS_TF_a5i3_translucent,
    NDS_TF_direct
};

enum NDS_TEX_COORD_TRANSFORM_MODE {
    NDS_TCTM_none,
    NDS_TCTM_texcoord,
    NDS_TCTM_normal,
    NDS_TCTM_vertex
};

struct NDS_GE_VTX_list_node
{
    struct NDS_GE_VTX_list_node_data {
        i32 xyzw[4];
        u32 processed;
        u16 vram_ptr;
        u32 vtx_parent;
        u32 w_normalized;

        i32 uv[2];
        u32 color[3];
    } data;


    u32 poolclear;
    struct NDS_GE_VTX_list_node *prev;
    struct NDS_GE_VTX_list_node *next;

};

#define NDS_GE_VTX_LIST_MAX 16

struct NDS_GE_VTX_list {
    struct NDS_GE_VTX_list_node *first, *last;
    i32 len;

    struct NDS_GE_VTX_list_node pool[NDS_GE_VTX_LIST_MAX];
    u16 pool_bitmap;
};


union NDS_GE_TEX_PARAM {
    struct {
        u32 vram_offset : 16;
        u32 repeat_s : 1;
        u32 repeat_t : 1;
        u32 flip_s : 1;
        u32 flip_t : 1;
        u32 sz_s : 3;
        u32 sz_t : 3;
        enum NDS_TEX_FORMAT format : 3;
        u32 color0_is_transparent : 1;
        enum NDS_TEX_COORD_TRANSFORM_MODE texture_coord_transform_mode : 2;
    };
    u32 u;
};

union NDS_GE_POLY_ATTR {
    struct {
        u32 light0_enable : 1;
        u32 light1_enable : 1;
        u32 light2_enable : 1;
        u32 light3_enable : 1;
        enum NDS_GE_POLY_MODE mode : 2; // 0 modulation, 1 decal, 2 toon/hightlight, 3 shadow
        u32 render_back : 1; // line segments are always rendered
        u32 render_front : 1;
        u32 _un1 : 3;
        u32 translucent_set_new_depth : 1;
        u32 render_if_intersect_far_plane : 1;
        u32 render_if_1dot_behind_DISP_1DOT_DEPTH : 1;
        enum NDS_RE_DEPTH_TEST_MODE depth_test_mode : 1;
        u32 fog_enable : 1;
        u32 alpha : 5; // 0 = wire-frame, 1-30= translucent, 31 = solid
        u32 _un2 : 3;
        u32 poly_id : 6;
    };
    u32 u;
};

struct NDS;
struct NDS_RE_POLY;
struct NDS_RE_TEX_SAMPLER {
    char *tex_ptr;
    u32 tex_addr;
    u32 tex_slot;
    u32 color0_is_transparent;
    u32 tex_slot1_addr;
    u32 pltt_base;
    u32 s_size, t_size; // total size
    u32 s_max_fp, t_max_fp;
    u32 s_size_fp, t_size_fp;
    u32 s_fp_mask, t_fp_mask;
    u32 s_sub_shift, t_sub_shift;
    u32 s_flip, t_flip; // flip every second
    u32 s_repeat, t_repeat;
    u32 filled_out;

    void (*sample)(struct NDS *, struct NDS_RE_TEX_SAMPLER *, struct NDS_RE_POLY *, u32 s, u32 t, u32 *r, u32 *g, u32 *b, u32 *a);
};


struct NDS_RE_POLY  { // no more __attribute__((packed))
    union NDS_GE_POLY_ATTR attr; // 32 bits
    union NDS_GE_TEX_PARAM tex_param;

    u32 pltt_base;
    u32 w_normalization_left;
    u32 w_normalization_right;
    struct NDS_GE_VTX_list vertex_list;
    u32 front_facing;
    u32 winding_order;

    struct NDS_GE_VTX_list_node *highest_vertex;
    i32 min_y, max_y;
    u16 edge_r_bitfield;

    struct NDS_RE_TEX_SAMPLER sampler;
};

struct NDS_RE_VERTEX { // 24 bytes
    u16 xx, yy;
    i32 ww, zz;
    i32 uv[2];
    i32 color[3]; // RGB!

    i32 original_w;

    i32 lr, lg, lb;
};

struct NDS_GE_BUFFERS {
    struct NDS_RE_POLY polygon[2048];
    struct NDS_RE_VERTEX vertex[6144];
    u32 polygon_index;
    u32 vertex_index;

    u32 translucent_y_sorting_manual;
    u32 depth_buffering_w;
};

struct NDS_RE_LINEBUFFER {
    u16 rgb_top[256];
    u16 rgb_bottom[256];
    u16 alpha[256];
    u16 poly_id[256];
    union NDS_GE_TEX_PARAM tex_param[256];
    i32 depth[256];
};

struct NDS_RE {
    u32 enable;
    struct {
        struct {
            u16 COLOR;
            u32 fog_to_rear_plane;
            u16 alpha;
            u16 poly_id;
            u32 depth;
        } CLEAR;

        struct {
            i32 TABLE[32];
            u32 COLOR_r, COLOR_g, COLOR_b;
            u32 OFFSET;
        } FOG;

        union {
            struct {
                u32 texture_mapping : 1;
                u32 polygon_attr_shading : 1;
                u32 alpha_testing : 1;
                u32 alpha_blending : 1;
                u32 anti_aliasing : 1;
                u32 edge_marking : 1;
                u32 fog_color_alpha_mode : 1;
                u32 fog_master_enable : 1;
                u32 fog_depth_shift : 4;
                u32 rdlines_underflow : 1;
                u32 poly_vtx_ram_overflow : 1;
                u32 rear_plane_mode : 1;
            };
            u32 u;
        } DISP3DCNT;

        struct {
            i32 x0, y0, x1, y1, width, height;
        } viewport;

        i32 SHININESS[128];
        i32 TOON_TABLE_r[32];
        i32 TOON_TABLE_g[32];
        i32 TOON_TABLE_b[32];
        i32 EDGE_TABLE_r[32];
        i32 EDGE_TABLE_g[32];
        i32 EDGE_TABLE_b[32];
        u8 ALPHA_TEST_REF;
        u32 CLRIMAGE_OFFSET;
    } io;

    struct {
        struct NDS_RE_LINEBUFFER linebuffer[192];
    } out;
};

struct  NDS_GE_CMD_QUEUE_entry
{
    u32 cmd;
    u32 num_params_left;
};

struct NDS_GE {
    struct NDS_GE_BUFFERS buffers[2];
    u32 enable;
    u32 ge_has_buffer;

    struct {
        u32 data[64];
    } cur_cmd;

    struct {
        struct NDS_GE_FIFO_entry items[512]; // real FIFO is 256, but we want more for reasons.
        // If we go over 256 items, we'll lock the CPU until we're under 256
        u32 head, tail;
        i32 len;
        u32 total_complete_cmds; // +1 every time we finish a command, -1 every time we take one
        u32 pausing_cpu;
        u32 cmd_scheduled;

        u32 waiting_for_cmd;
        u32 cur_cmd;

        struct  {
            struct NDS_GE_CMD_QUEUE_entry items[4];
            u32 head;
            u32 len;
        }cmd_queue;
    } fifo;

    struct {
        struct {
            i32 projection_ptr;
            i32 coordinate_direction_ptr;
            i32 texture_ptr;

            struct NDS_GE_matrix coordinate[32];
            struct NDS_GE_matrix direction[32];
            struct NDS_GE_matrix projection[2];
            struct NDS_GE_matrix texture[2];
        } stacks;
        struct NDS_GE_matrix coordinate, texture, projection, direction;
        struct NDS_GE_matrix clip;
    } matrices;

    struct {
        union {
            struct {
                u32 texture_mapping : 1;
                u32 attr_shading : 1;
                u32 alpha_test : 1;
                u32 alpha_blend : 1;
                u32 anti_alias : 1;
                u32 edge_mark : 1;
                u32 fog_color_alpha_mode : 1;
                u32 fog_enable : 1;
                u32 fog_depth_shift : 4;
                u32 rdlines_underflow : 1;
                u32 ram_overflow : 1;
                u32 rear_plane_mode : 1;
            };
            u32 u;
        } DISP3DCNT;

        union {
            struct {
                u32 test_busy : 1;
                u32 box_test_result : 1;
                u32 _r1 : 6;
                u32 position_vector_matrix_stack_level: 5;
                u32 projection_matrix_stack_level : 1;
                u32 matrix_stack_busy : 1;
                u32 matrix_stack_over_or_underflow_error : 1;
                u32 cmd_fifo_len : 8; // 0...255
                u32 cmd_fifo_full : 1;
                u32 cmd_fifo_less_than_half_full : 1;
                u32 cmd_fifo_empty : 1;
                u32 ge_busy : 1;
                u32 _r2 : 2;
                u32 cmd_fifo_irq_mode : 2;
            };
            u32 u;
        } GXSTAT;

        struct {
            u32 pos;
            u32 num_cmds; // Number of commands, 1-4
            u32 cmds[4];
            u32 cmd_pos[4];
            u32 cmd_num_params[4];
            u32 buf[160]; // 132 is the max that should be written at once actually
            u32 total_len; // Total length of all 4 commands
        } fifo_in;
        u32 doing_vertices;
        u32 swap_buffers;

        u32 MTX_MODE;
    } io;

    struct {
        struct {
            struct {
                union NDS_GE_POLY_ATTR attr;
            } on_vtx_start;

            struct {
                union NDS_GE_POLY_ATTR attr;
                u32 num_lights; // set from attr when attr is latched
                union NDS_GE_TEX_PARAM tex_param;
                u32 pltt_base;
            } current;
        } poly;

        struct {
            u32 color[3]; // 6-bit R, 6-bit G, 6-bit B
            i32 S, T;
            i16 x,y,z,w;

            struct NDS_GE_VTX_list input_list;
        } vtx;

        struct {
            enum {
                NDS_GEM_SEPERATE_TRIANGLES,
                NDS_GEM_SEPERATE_QUADS,
                NDS_GEM_TRIANGLE_STRIP,
                NDS_GEM_QUAD_STRIP
            } mode;
        } vtx_strip;
    } params;

    struct {
        struct NDS_GE_LIGHT {
            i32 direction[4];
            i32 halfway[4];
            u32 color[3];
        } light[4];

        u32 shininess_enable;
        struct {
            u32 diffuse[3];
            u32 ambient[3];
            u32 specular_reflection[3];
            u32 specular_emission[3];
        } material_color;
    }lights;

    struct {
        u32 pos_test[4];
        u32 vector[2];
    } results;
    u32 winding_order;
};

struct NDS;
void NDS_GE_init(struct NDS *);
void NDS_GE_reset(struct NDS *);
u32 NDS_GE_check_irq(struct NDS *);
void NDS_GE_write(struct NDS *, u32 addr, u32 sz, u32 val);
u32 NDS_GE_read(struct NDS *, u32 addr, u32 sz);
void NDS_GE_vblank_up(struct NDS *);

#endif //JSMOOCH_EMUS_NDS_GE_H
