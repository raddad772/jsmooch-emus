//
// Created by . on 3/9/25.
//

#pragma once

#include "component/gpu/cdp1861/cdp1861.h"
#include "helpers/int.h"
#include "helpers/scheduler.h"

namespace NDS {
struct core;
namespace GFX {



enum CMDS {
    CMD_NOP = 0x00,
    CMD_MTX_MODE = 0x10,
    CMD_MTX_PUSH = 0x11,
    CMD_MTX_POP = 0x12,
    CMD_MTX_STORE = 0x13,
    CMD_MTX_RESTORE = 0x14,
    CMD_MTX_IDENTITY = 0x15,
    CMD_MTX_LOAD_4x4 = 0x16,
    CMD_MTX_LOAD_4x3 = 0x17,
    CMD_MTX_MULT_4x4 = 0x18,
    CMD_MTX_MULT_4x3 = 0x19,
    CMD_MTX_MULT_3x3 = 0x1A,
    CMD_MTX_SCALE = 0x1B,
    CMD_MTX_TRANS = 0x1C,
    CMD_COLOR = 0x20,
    CMD_NORMAL = 0x21,
    CMD_TEXCOORD = 0x22,
    CMD_VTX_16 = 0x23,
    CMD_VTX_10 = 0x24,
    CMD_VTX_XY = 0x25,
    CMD_VTX_XZ = 0x26,
    CMD_VTX_YZ = 0x27,
    CMD_VTX_DIFF = 0x28,
    CMD_POLYGON_ATTR = 0x29,
    CMD_TEXIMAGE_PARAM = 0x2A,
    CMD_PLTT_BASE = 0x2B,
    CMD_DIF_AMB = 0x30,
    CMD_SPE_EMI = 0x31,
    CMD_LIGHT_VECTOR = 0x32,
    CMD_LIGHT_COLOR = 0x33,
    CMD_SHININESS = 0x34,
    CMD_BEGIN_VTXS = 0x40,
    CMD_END_VTXS = 0x41,
    CMD_SWAP_BUFFERS = 0x50,
    CMD_VIEWPORT = 0x60,
    CMD_BOX_TEST = 0x70,
    CMD_POS_TEST = 0x71,
    CMD_VEC_TEST = 0x72,
};

typedef i32 MATRIX[16];

struct FIFO_entry {
    u8 cmd{};
    u32 data{};
};

enum POLY_MODE {
    NDS_GE_PM_modulation,
    NDS_GE_PM_decal,
    NDS_GE_PM_toon_hightlight,
    NDS_GE_PM_shadow
};

enum DEPTH_TEST_MODE {
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


struct VTX_list_node_data {
    i32 xyzw[4]{};
    u32 processed{};
    u16 vram_ptr{};
    u32 vtx_parent{};
    u32 w_normalized{};

    i32 uv[2]{};
    i32 color[3]{};
};

struct VTX_list_node
{
    VTX_list_node_data data{};

    u32 poolclear{};
    VTX_list_node *prev{};
    VTX_list_node *next{};
};

#define NDS_GE_VTX_LIST_MAX 16

struct VTX_list {
    VTX_list_node *first{}, *last{};
    i32 len{};

    VTX_list_node pool[NDS_GE_VTX_LIST_MAX]{};
    u16 pool_bitmap{};
    void init();
};

union TEX_PARAM {
    struct {
        u32 vram_offset : 16;
        u32 repeat_s : 1;
        u32 repeat_t : 1;
        u32 flip_s : 1;
        u32 flip_t : 1;
        u32 sz_s : 3;
        u32 sz_t : 3;
        NDS_TEX_FORMAT format : 3;
        u32 color0_is_transparent : 1;
        NDS_TEX_COORD_TRANSFORM_MODE texture_coord_transform_mode : 2;
    };
    u32 u{};
};

union POLY_ATTR {
    struct {
        u32 light0_enable : 1;
        u32 light1_enable : 1;
        u32 light2_enable : 1;
        u32 light3_enable : 1;
        POLY_MODE mode : 2; // 0 modulation{}, 1 decal{}, 2 toon/hightlight{}, 3 shadow
        u32 render_back : 1; // line segments are always rendered
        u32 render_front : 1;
        u32 _un1 : 3;
        u32 translucent_set_new_depth : 1;
        u32 render_if_intersect_far_plane : 1;
        u32 render_if_1dot_behind_DISP_1DOT_DEPTH : 1;
        DEPTH_TEST_MODE depth_test_mode : 1;
        u32 fog_enable : 1;
        u32 alpha : 5; // 0 = wire-frame{}, 1-30= translucent{}, 31 = solid
        u32 _un2 : 3;
        u32 poly_id : 6;
    };
    u32 u{};
};


struct POLY;
struct RE;

struct TEX_SAMPLER {
    char *tex_ptr{};
    u32 tex_addr{};
    u32 tex_slot{};
    u32 tex_slot1_addr{};
    u32 pltt_base{};
    u32 s_size{}, t_size{}; // total size
    u32 s_mask{}, t_mask{};
    u32 alpha0{};
    u32 s_size_fp{}, t_size_fp{};
    u32 s_flip{}, t_flip{}; // flip every second
    u32 s_repeat{}, t_repeat{};
    u32 filled_out{};

    void (*sample)(NDS::core *, TEX_SAMPLER *, POLY *, u32 s, u32 t, u32 *r, u32 *g, u32 *b, u32 *a){};
};


struct POLY  { // no more __attribute__((packed))
    POLY_ATTR attr{}; // 32 bits
    TEX_PARAM tex_param{};

    u32 sorting_key{};
    u32 is_translucent{}, is_shadow_mask{}, is_shadow{};
    u32 pltt_base{};
    u32 w_normalization_left{};
    u32 w_normalization_right{};
    u32 vertex_mode{};
    VTX_list vertex_list{};
    u32 front_facing{};
    u32 winding_order{};

    VTX_list_node *highest_vertex{};
    i32 min_y{}, max_y{};
    u16 edge_r_bitfield{};

    TEX_SAMPLER sampler{};
};

struct VERTEX { // 24 bytes
    u16 xx{}, yy{};
    i32 ww{}, zz{};
    i32 uv[2]{};
    i32 color[3]{}; // RGB!

    i32 original_w{};

    i32 lr{}, lg{}, lb{};
};

struct BUFFERS {
    POLY polygon[2048]{};
    VERTEX vertex[6144]{};
    u32 polygon_index{};
    u32 vertex_index{};

    bool translucent_y_sorting_manual{};
    bool depth_buffering_w{};
};

union EXTRA_ATTR {
    struct {
        u32 vertex_mode;
        u32 has_px;
        u32 was_blended;
        u32 shading_mode;
    };
    u32 u{};
};

struct LINEBUFFER {
    u8 stencil[256]{};
    bool has[256]{};
    u32 rgb[256]{};
    u16 alpha[256]{};
    u16 poly_id[256]{};
    EXTRA_ATTR extra_attr[256]{};
    TEX_PARAM tex_param[256]{};
    u32 depth[256]{};
};

struct POLY_LIST {
    u32 len{};
    POLY *items[2048]{};
    u32 num_opaque{};
    i32 num_translucent{};
};

struct GE;
struct RE {
    explicit RE(NDS::core *parent) : bus(parent) {}
    NDS::core *bus;
    void render_frame();
    void copy_and_sort_list(BUFFERS *b);
    GE *ge;
    void reset();
    void clear_line(LINEBUFFER *l);
    void render_line(BUFFERS *b, i32 line_num);

    u32 enable{};
    POLY_LIST render_list{};
    struct {
        struct {
            u32 COLOR{};
            u32 fog_to_rear_plane{};
            u16 alpha{};
            u16 poly_id{};
            u32 depth{};
        } CLEAR{};

        struct {
            i32 TABLE[32]{};
            u32 COLOR_r{}, COLOR_g{}, COLOR_b{};
            u32 OFFSET{};
        } FOG{};

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
                u32 rear_plane_is_bitmap : 1;
            };
            u32 u{};
        } DISP3DCNT{};

        u32 viewport[6]{};

        u8 SHININESS[128]{};
        u32 TOON_TABLE_r[32]{};
        u32 TOON_TABLE_g[32]{};
        u32 TOON_TABLE_b[32]{};
        i32 EDGE_TABLE_r[32]{};
        i32 EDGE_TABLE_g[32]{};
        i32 EDGE_TABLE_b[32]{};
        u8 ALPHA_TEST_REF{};
        u32 CLRIMAGE_OFFSET{};
    } io{};

    struct {
        LINEBUFFER linebuffer[192]{};
    } out{};
};

struct  CMD_QUEUE_entry
{
    u32 cmd{};
    u32 num_params_left{};
};

struct LIGHT {
    i16 direction[4]{};
    u32 color[3]{};
};

struct GE;
struct FIFO {
    explicit FIFO(GE *parent) : ge(parent) {}
    GE *ge;
    FIFO_entry items[512]{}; // real FIFO is 256{}, but we want more for reasons.
    void push(u32 cmd, u32 val, bool from_dma);
    void parse_cmd(u32 val, bool from_dma);
    void drain_cmd_queue(bool from_dma);

    // If we go over 256 items{}, we'll lock the CPU until we're under 256
    u32 head{}, tail{};
    i32 len{};
    u32 total_complete_cmds{}; // +1 every time we finish a command{}, -1 every time we take one
    bool pausing_cpu{};
    u32 cmd_scheduled{};

    bool waiting_for_cmd{};
    u32 cur_cmd{};

    struct  {
        CMD_QUEUE_entry items[4]{};
        u32 head{};
        u32 len{};
    }cmd_queue{};

    void update_len(bool from_dma, bool did_add);
};

enum P_MODE {
    SEPERATE_TRIANGLES = 0,
    SEPERATE_QUADS = 1,
    TRIANGLE_STRIP = 2,
    QUAD_STRIP = 3
};

struct RE;
struct GE {
    explicit GE(NDS::core *parent, scheduler_t *scheduler_in);
    NDS::core *bus;
    u32 fifo_pop_full_cmd();
    void handle_cmd();
    void fifo_write_direct(u32 val, bool from_dma, bool from_addr);
    void fifo_write_from_addr(CMDS cmd, u32 val);
    void write(u32 addr, u8 sz, u32 val);
    u32 read_results(u32 addr, u8 sz);
    u32 read(u32 addr, u8 sz);
    void vblank_up();

    RE *re;
    void reset();
    [[nodiscard]] i32 get_num_cycles(u32 cmd) const;
    u32 check_irq() const;
    void write_fog_table(u32 addr, u8 sz, u32 val);
    void write_edge_table(u32 addr, u8 sz, u32 val);
    void write_toon_table(u32 addr, u8 sz, u32 val);

    BUFFERS buffers[2]{};
    scheduler_t *scheduler;
    bool enable{};
    u8 ge_has_buffer{};
    bool clip_mtx_dirty{};

    struct {
        u32 data[64]{};
    } cur_cmd{};

    FIFO fifo{this};

    struct {
        struct {
            i32 projection_ptr{};
            i32 position_vector_ptr{};
            i32 texture_ptr{};

            MATRIX position[32]{};
            MATRIX vector[32]{};
            MATRIX projection[16]{};
            MATRIX texture[16]{};
        } stacks{};
        MATRIX position{}, texture{}, projection{}, vector{};
        MATRIX clip{};
    } matrices{};

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
            u32 u{};
        } DISP3DCNT{};

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
            u32 u{};
        } GXSTAT{};

        struct {
            u32 pos{};
            u32 num_cmds{}; // Number of commands{}, 1-4
            u32 cmds[4]{};
            u32 cmd_pos[4]{};
            u32 cmd_num_params[4]{};
            u32 buf[160]{}; // 132 is the max that should be written at once actually
            u32 total_len{}; // Total length of all 4 commands
        } fifo_in{};
        u32 doing_vertices{};
        u32 swap_buffers{};

        u32 MTX_MODE{};
    } io{};

    struct {
        struct {
            struct {
                POLY_ATTR attr{};
            } on_vtx_start{};

            struct {
                POLY_ATTR attr{};
                u32 num_lights{}; // set from attr when attr is latched
                TEX_PARAM tex_param{};
                u32 pltt_base{};
            } current{};
        } poly{};

        struct {
            u32 color[3]{}; // 6-bit R{}, 6-bit G{}, 6-bit B
            i16 uv[2]{};
            i16 original_uv[2]{};
            i16 x{},y{},z{},w{};

            VTX_list input_list{};
        } vtx{};

        struct {
            P_MODE mode{};
        } vtx_strip{};
    } params{};

    struct {
        LIGHT light[4]{};

        u32 shininess_enable{};
        i16 normal[3]{};
        struct {
            u32 diffuse[3]{};
            u32 ambient[3]{};
            u32 reflection[3]{};
            u32 emission[3]{};
            i32 reciprocal[3]{};
        } material_color{};
    }lights{};

    struct {
        i32 pos_test[4]{};
        u32 vector[2]{};
    } results{};
    u32 winding_order{};

public:
    void cmd_END_VTXS();
    void cmd_NOP();
    void cmd_MTX_MODE();
    void cmd_MTX_PUSH();
    void cmd_MTX_POP();
    void cmd_MTX_STORE();
    void cmd_MTX_RESTORE();
    void cmd_MTX_IDENTITY();
    void cmd_MTX_LOAD_4x4();
    void cmd_MTX_LOAD_4x3();
    void cmd_SHININESS();
    void cmd_MTX_MULT_4x4();
    void cmd_MTX_MULT_4x3();
    void cmd_MTX_MULT_3x3();
    void cmd_MTX_SCALE();
    void cmd_MTX_TRANS();
    void cmd_COLOR();
    void cmd_NORMAL();
    void cmd_TEXCOORD();
    void cmd_VTX_16();
    void cmd_VTX_DIFF();
    void cmd_VTX_XY();
    void cmd_VTX_XZ();
    void cmd_VTX_YZ();
    void cmd_VTX_10();
    void cmd_POS_TEST();
    void cmd_BOX_TEST();
    void cmd_SWAP_BUFFERS();
    void cmd_POLYGON_ATTR();
    void cmd_VIEWPORT();
    void cmd_TEXIMAGE_PARAM();
    void cmd_BEGIN_VTXS();
    void cmd_PLTT_BASE();
    void cmd_LIGHT_VECTOR();
    void cmd_DIF_AMB();
    void cmd_SPE_EMI();
    void cmd_LIGHT_COLOR();


    void terminate_poly_strip();

    void transform_vertex_on_ingestion(VTX_list_node *node);
    void clip_verts_on_plane(u32 comp, u32 attribs, VTX_list *vertices);
    void clip_verts(POLY *out);
    void calculate_clip_matrix();
    u32 commit_vertex(VTX_list_node *v, i32 xx, i32 yy, i32 zz, i32 ww, i32 *uv, u32 cr, u32 cg, u32 cb);
    void finalize_verts_and_get_first_addr(POLY *poly);
    void evaluate_edges(POLY *poly, u32 expected_winding_order);
    void ingest_poly(u32 in_winding_order);
    void ingest_vertex();
};

}
}